/*
 * Copyright (c) 2018 Google LLC.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <device.h>
#include <soc.h>
#include <dma.h>

#define LOG_LEVEL CONFIG_DMA_LOG_LEVEL
#include <logging/log.h>
LOG_MODULE_REGISTER(dma_sam0);

typedef void (*dma_callback)(void *callback_arg, u32_t channel,
			     int error_code);

struct dma_sam0_channel {
	dma_callback cb;
	void *cb_arg;
};

struct dma_sam0_data {
	__aligned(16) DmacDescriptor descriptors[DMAC_CH_NUM];
	__aligned(16) DmacDescriptor descriptors_wb[DMAC_CH_NUM];
	struct dma_sam0_channel channels[DMAC_CH_NUM];
};

#define DEV_DATA(dev) \
	((struct dma_sam0_data *const)(dev)->driver_data)


/* Handles DMA interrupts and dispatches to the individual channel */
static void dma_sam0_isr(void *arg)
{
	struct device *dev = arg;
	struct dma_sam0_data *data = DEV_DATA(dev);
	struct dma_sam0_channel *chdata;
	u16_t pend = DMAC->INTPEND.reg;
	u32_t channel;

	/* Acknowledge all interrupts for the channel in pend */
	DMAC->INTPEND.reg = pend;

	channel = (pend & DMAC_INTPEND_ID_Msk) >> DMAC_INTPEND_ID_Pos;
	chdata = &data->channels[channel];

	if (pend & DMAC_INTPEND_TERR) {
		if (chdata->cb) {
			chdata->cb(chdata->cb_arg, channel, -DMAC_INTPEND_TERR);
		}
	} else if (pend & DMAC_INTPEND_TCMPL) {
		if (chdata->cb) {
			chdata->cb(chdata->cb_arg, channel, 0);
		}
	}

	/*
	 * If more than one channel is pending, we'll just immediately
	 * interrupt again and handle it through a different INTPEND value.
	 */
}

/* Configure a channel */
static int dma_sam0_config(struct device *dev, u32_t channel,
			   struct dma_config *config)
{
	struct dma_sam0_data *data = DEV_DATA(dev);
	DmacDescriptor *desc = &data->descriptors[channel];
	struct dma_block_config *block = config->head_block;
	struct dma_sam0_channel *channel_control;
	DMAC_BTCTRL_Type btctrl = {.reg = 0};
	int key;

	if (channel >= DMAC_CH_NUM) {
		LOG_ERR("Unsupported channel");
		return -EINVAL;
	}
	if (config->block_count > 1) {
		LOG_ERR("Chained transfers not supported");
		/* TODO: add support for chained transfers. */
		return -ENOTSUP;
	}

	if (config->dma_slot >= DMAC_TRIG_NUM) {
		LOG_ERR("Invalid trigger number");
		return -EINVAL;
	}

	/* Lock and page in the channel configuration */
	key = irq_lock();

#ifdef DMAC_CHID_ID
	/* Select the channel for configuration */
	DMAC->CHID.reg = DMAC_CHID_ID(channel);
	DMAC->CHCTRLA.reg = 0;

	/* Connect the peripheral trigger */
	if (config->channel_direction == MEMORY_TO_MEMORY) {
		/*
		 * A single software trigger will start the
		 * transfer
		 */
		DMAC->CHCTRLB.reg = DMAC_CHCTRLB_TRIGACT_TRANSACTION |
			DMAC_CHCTRLB_TRIGSRC(config->dma_slot);
	} else {
		/* One peripheral trigger per beat */
		DMAC->CHCTRLB.reg = DMAC_CHCTRLB_TRIGACT_BEAT |
			DMAC_CHCTRLB_TRIGSRC(config->dma_slot);
	}

	/* Set the priority */
	if (config->channel_priority >= DMAC_LVL_NUM) {
		LOG_ERR("Invalid priority");
		goto inval;
	}
	DMAC->CHCTRLB.bit.LVL = config->channel_priority;

	/* Enable the interrupts */
	DMAC->CHINTENSET.reg = DMAC_CHINTENSET_TCMPL;
	if (!config->error_callback_en) {
		DMAC->CHINTENSET.reg = DMAC_CHINTENSET_TERR;
	} else {
		DMAC->CHINTENCLR.reg = DMAC_CHINTENSET_TERR;
	}
	DMAC->CHINTFLAG.reg = DMAC_CHINTFLAG_TERR | DMAC_CHINTFLAG_TCMPL;
#else
	/* Channels have separate configuration registers */
	DmacChannel * chcfg = &DMAC->Channel[channel];

	if (config->channel_direction == MEMORY_TO_MEMORY) {
		/*
		 * A single software trigger will start the
		 * transfer
		 */
		chcfg->CHCTRLA.reg = DMAC_CHCTRLA_TRIGACT_TRANSACTION |
				DMAC_CHCTRLA_TRIGSRC(config->dma_slot);
	} else {
		/* One peripheral trigger per beat */
		chcfg->CHCTRLA.reg = DMAC_CHCTRLA_TRIGACT_BURST |
				DMAC_CHCTRLA_TRIGSRC(config->dma_slot);
	}

	/* Set the priority */
	if (config->channel_priority >= DMAC_LVL_NUM) {
		LOG_ERR("Invalid priority");
		goto inval;
	}
	chcfg->CHPRILVL.bit.PRILVL = config->channel_priority;

	/* Set the burst length */
	if (config->source_burst_length != config->dest_burst_length) {
		LOG_ERR("Source and destination burst lengths must be equal");
		goto inval;
	}
	if (config->source_burst_length > 16) {
		LOG_ERR("Invalid burst length");
		goto inval;
	}
	if (config->source_burst_length > 0) {
		chcfg->CHCTRLA.reg |= DMAC_CHCTRLA_BURSTLEN(
				config->source_burst_length - 1);
	}

	/* Enable the interrupts */
	chcfg->CHINTENSET.reg = DMAC_CHINTENSET_TCMPL;
	if (!config->error_callback_en) {
		chcfg->CHINTENSET.reg = DMAC_CHINTENSET_TERR;
	} else {
		chcfg->CHINTENCLR.reg = DMAC_CHINTENSET_TERR;
	}
	chcfg->CHINTFLAG.reg = DMAC_CHINTFLAG_TERR | DMAC_CHINTFLAG_TCMPL;
#endif

	/* Set the beat (single transfer) size */
	if (config->source_data_size != config->dest_data_size) {
		LOG_ERR("Source and destination data sizes must be equal");
		goto inval;
	}
	switch (config->source_data_size) {
	case 1:
		btctrl.bit.BEATSIZE = DMAC_BTCTRL_BEATSIZE_BYTE_Val;
		break;
	case 2:
		btctrl.bit.BEATSIZE = DMAC_BTCTRL_BEATSIZE_HWORD_Val;
		break;
	case 4:
		btctrl.bit.BEATSIZE = DMAC_BTCTRL_BEATSIZE_WORD_Val;
		break;
	default:
		LOG_ERR("Invalid data size");
		goto inval;
	}

	/* Set up the one and only block */
	desc->BTCNT.reg = block->block_size / config->source_data_size;
	desc->DESCADDR.reg = 0;

	/* Set the automatic source / dest increment */
	switch (block->source_addr_adj) {
	case DMA_ADDR_ADJ_INCREMENT:
		desc->SRCADDR.reg = block->source_address + block->block_size;
		btctrl.bit.SRCINC = 1;
		break;
	case DMA_ADDR_ADJ_NO_CHANGE:
		desc->SRCADDR.reg = block->source_address;
		break;
	default:
		LOG_ERR("Invalid source increment");
		goto inval;
	}

	switch (block->dest_addr_adj) {
	case DMA_ADDR_ADJ_INCREMENT:
		desc->DSTADDR.reg = block->dest_address + block->block_size;
		btctrl.bit.DSTINC = 1;
		break;
	case DMA_ADDR_ADJ_NO_CHANGE:
		desc->DSTADDR.reg = block->dest_address;
		break;
	default:
		LOG_ERR("Invalid destination increment");
		goto inval;
	}

	btctrl.bit.VALID = 1;
	desc->BTCTRL = btctrl;

	channel_control = &data->channels[channel];
	channel_control->cb = config->dma_callback;
	channel_control->cb_arg = config->callback_arg;

	LOG_DBG("Configured channel %d for %08X to %08X (%u)",
			channel,
			block->source_address,
			block->dest_address,
			block->block_size);

	irq_unlock(key);
	return 0;

inval:
	irq_unlock(key);
	return -EINVAL;
}

static int dma_sam0_start(struct device *dev, u32_t channel)
{
	int key = irq_lock();

	ARG_UNUSED(dev);

#ifdef DMAC_CHID_ID
	DMAC->CHID.reg = channel;
	DMAC->CHCTRLA.reg = DMAC_CHCTRLA_ENABLE;

	if (DMAC->CHCTRLB.bit.TRIGSRC == 0) {
		/* Trigger via software */
		DMAC->SWTRIGCTRL.reg = 1 << channel;
	}
#else
	DmacChannel * chcfg = &DMAC->Channel[channel];

	chcfg->CHCTRLA.bit.ENABLE = 1;

	if (chcfg->CHCTRLA.bit.TRIGSRC == 0) {
		/* Trigger via software */
		DMAC->SWTRIGCTRL.reg = 1 << channel;
	}
#endif

	irq_unlock(key);

	return 0;
}

static int dma_sam0_stop(struct device *dev, u32_t channel)
{
	int key = irq_lock();

	ARG_UNUSED(dev);

#ifdef DMAC_CHID_ID
	DMAC->CHID.reg = channel;
	DMAC->CHCTRLA.reg = 0;
#else
	DmacChannel * chcfg = &DMAC->Channel[channel];

	chcfg->CHCTRLA.bit.ENABLE = 0;
#endif

	irq_unlock(key);

	return 0;
}

static int dma_sam0_reload(struct device *dev, u32_t channel,
			   u32_t src, u32_t dst, size_t size)
{
	struct dma_sam0_data *data = DEV_DATA(dev);
	DmacDescriptor *desc = &data->descriptors[channel];
	int key = irq_lock();

	switch (desc->BTCTRL.bit.BEATSIZE) {
	case DMAC_BTCTRL_BEATSIZE_BYTE_Val:
		desc->BTCNT.reg = size;
		break;
	case DMAC_BTCTRL_BEATSIZE_HWORD_Val:
		desc->BTCNT.reg = size / 2;
		break;
	case DMAC_BTCTRL_BEATSIZE_WORD_Val:
		desc->BTCNT.reg = size / 4;
		break;
	default:
		goto inval;
	}

	if (desc->BTCTRL.bit.SRCINC) {
		desc->SRCADDR.reg = src + size;
	} else {
		desc->SRCADDR.reg = src;
	}

	if (desc->BTCTRL.bit.DSTINC) {
		desc->DSTADDR.reg = dst + size;
	} else {
		desc->DSTADDR.reg = dst;
	}

	LOG_DBG("Reloaded channel %d for %08X to %08X (%u)",
			channel, src, dst, size);

	irq_unlock(key);
	return 0;

inval:
	irq_unlock(key);
	return -EINVAL;
}

DEVICE_DECLARE(dma_sam0_0);

#define DMA_SAM0_IRQ_CONNECT(n)						     \
	do {								     \
		IRQ_CONNECT(DT_ATMEL_SAM0_DMAC_0_IRQ_ ## n,		     \
				DT_ATMEL_SAM0_DMAC_0_IRQ_ ## n ## _PRIORITY, \
				dma_sam0_isr, DEVICE_GET(dma_sam0_0), 0);    \
		irq_enable(DT_ATMEL_SAM0_DMAC_0_IRQ_ ## n);		     \
	} while (0)

static int dma_sam0_init(struct device *dev)
{
	struct dma_sam0_data *data = DEV_DATA(dev);

	/* Enable clocks. */
#ifdef MCLK
	MCLK->AHBMASK.bit.DMAC_ = 1;
#else
	PM->AHBMASK.bit.DMAC_ = 1;
	PM->APBBMASK.bit.DMAC_ = 1;
#endif

	/* Set up the descriptor and write back addresses */
	DMAC->BASEADDR.reg = (uintptr_t)&data->descriptors;
	DMAC->WRBADDR.reg = (uintptr_t)&data->descriptors_wb;

	/* Statically map each level to the same numeric priority */
	DMAC->PRICTRL0.reg =
		DMAC_PRICTRL0_LVLPRI0(0) | DMAC_PRICTRL0_LVLPRI1(1) |
		DMAC_PRICTRL0_LVLPRI2(2) | DMAC_PRICTRL0_LVLPRI3(3);

	/* Enable the unit and enable all priorities */
	DMAC->CTRL.reg = DMAC_CTRL_DMAENABLE | DMAC_CTRL_LVLEN(0x0F);

#ifdef DT_ATMEL_SAM0_DMAC_0_IRQ_0
	DMA_SAM0_IRQ_CONNECT(0);
#endif
#ifdef DT_ATMEL_SAM0_DMAC_0_IRQ_1
	DMA_SAM0_IRQ_CONNECT(1);
#endif
#ifdef DT_ATMEL_SAM0_DMAC_0_IRQ_2
	DMA_SAM0_IRQ_CONNECT(2);
#endif
#ifdef DT_ATMEL_SAM0_DMAC_0_IRQ_3
	DMA_SAM0_IRQ_CONNECT(3);
#endif
#ifdef DT_ATMEL_SAM0_DMAC_0_IRQ_4
	DMA_SAM0_IRQ_CONNECT(4);
#endif

	return 0;
}

static struct dma_sam0_data dmac_data;

static const struct dma_driver_api dma_sam0_api = {
	.config = dma_sam0_config,
	.start = dma_sam0_start,
	.stop = dma_sam0_stop,
	.reload = dma_sam0_reload,
};

DEVICE_AND_API_INIT(dma_sam0_0, CONFIG_DMA_0_NAME, &dma_sam0_init,
		    &dmac_data, NULL, POST_KERNEL,
		    CONFIG_KERNEL_INIT_PRIORITY_DEVICE, &dma_sam0_api);
