/*
 * Copyright (c) 2019 Derek Hageman <hageman@inthat.cloud>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <soc.h>
#include <clock_control.h>

/*
 * Initialization priorities, the fixed oscillators come before anything
 * else, then all GCLKs other than 0, then the programmable oscillators,
 * finally GLK0 (and thus the core clock).
 */
#define SAM0_CLOCK_INIT_PRIORITY_FIXED_OSC	0
#define SAM0_CLOCK_INIT_PRIORITY_GCLK_ANY	1
#define SAM0_CLOCK_INIT_PRIORITY_PROG_OSC	2
#define SAM0_CLOCK_INIT_PRIORITY_GCLK_0		3

BUILD_ASSERT(CONFIG_SYSTEM_CLOCK_INIT_PRIORITY > \
	     SAM0_CLOCK_INIT_PRIORITY_GCLK_0);

#define SOC_ATMEL_SAM0_CLOCK_SUBSYS_TYPE_GCLK_IN		0x02000000U
#define SOC_ATMEL_SAM0_CLOCK_SUBSYS_GCLK_IN_ID_MASK		0x0000FFFFU
#define SOC_ATMEL_SAM0_CLOCK_SUBSYS_GCLK_IN_DIVSEL		0x00010000U

#define SOC_ATMEL_SAM0_CLOCK_SUBSYS_TYPE_FDPLL_IN		0x03000000U



static inline bool sam0_clock_is_gclk_output(clock_control_subsys_t sub_system)
{
	return ((u32_t)sub_system & SOC_ATMEL_SAM0_CLOCK_SUBSYS_TYPE_MASK) ==
			SOC_ATMEL_SAM0_CLOCK_SUBSYS_TYPE_GCLK_OUT;
}

static inline bool sam0_clock_is_gclk_input(clock_control_subsys_t sub_system)
{
	return ((u32_t)sub_system & SOC_ATMEL_SAM0_CLOCK_SUBSYS_TYPE_MASK) ==
			SOC_ATMEL_SAM0_CLOCK_SUBSYS_TYPE_GCLK_IN;
}

#ifdef SOC_ATMEL_SAM0_CLOCK_SUBSYS_TYPE_FDPLL_IN
static inline bool sam0_clock_is_fdpll_input(clock_control_subsys_t sub_system)
{
	return ((u32_t)sub_system & SOC_ATMEL_SAM0_CLOCK_SUBSYS_TYPE_MASK) ==
			SOC_ATMEL_SAM0_CLOCK_SUBSYS_TYPE_FDPLL_IN;
}
#endif


static inline u32_t sam0_clock_control_gclk_output_control(
		clock_control_subsys_t sub_system)
{
	return GCLK_CLKCTRL_ID((u32_t)sub_system &
			SOC_ATMEL_SAM0_CLOCK_SUBSYS_GCLK_OUT_ID_MASK);
}

struct sam0_gclk_config {
	u32_t clock_frequency;
	const char *clock_source;
	u8_t gen_id;
};


static void sam0_gclk_wait_gclk_synchronization(void)
{
	while (GCLK->STATUS.bit.SYNCBUSY) {
	}
}

static void sam0_flash_wait_init(u32_t ahb_frequency)
{
	/* SAMD21 Manual: 37.12 wait states for Vdd >= 2.7V */
	if (ahb_frequency <= 24000000) {
		NVMCTRL->CTRLB.bit.RWS = 0;
	} else {
		NVMCTRL->CTRLB.bit.RWS = 1;
	}
}

static void sam0_dividers_init(u32_t cpu_frequency)
{
	u32_t div = DT_ATMEL_SAM0_CLOCK_GCLK0_CLOCK_FREQUENCY / cpu_frequency;

	/* Only powers of two up to 128 are available */
	__ASSERT((div & (div - 1)) == 0, "invalid CPU clock frequency");
	div = 31U - __builtin_clz(div);
	__ASSERT(div <= 7, "invalid CPU clock frequency");

	sam0_flash_wait_init(cpu_frequency);

	PM->CPUSEL.bit.CPUDIV = div;
	PM->APBASEL.bit.APBADIV = div;
	PM->APBBSEL.bit.APBBDIV = div;
	PM->APBCSEL.bit.APBCDIV = div;
}

static void sam0_osc_ondemand(void)
{
	/*
	 * Set all enabled oscillators to ONDEMAND, so the unneeded ones
	 * can stop.
	 */
	SYSCTRL->OSC8M.bit.ONDEMAND = 1;

#if DT_ATMEL_SAM0_CLOCK_OSC32K_CLOCK_FREQUENCY
	SYSCTRL->OSC32K.bit.ONDEMAND = 1;
#endif
#if DT_ATMEL_SAM0_CLOCK_XOSC32K_CLOCK_FREQUENCY
	SYSCTRL->XOSC32K.bit.ONDEMAND = 1;
#endif
#if DT_ATMEL_SAM0_CLOCK_XOSC_CLOCK_FREQUENCY
	SYSCTRL->XOSC.bit.ONDEMAND = 1;
#endif
#if DT_ATMEL_SAM0_CLOCK_DFLL_CLOCK_FREQUENCY
	SYSCTRL->DFLLCTRL.bit.ONDEMAND = 1;
#endif
#if DT_ATMEL_SAM0_CLOCK_FDPLL_CLOCK_FREQUENCY
	SYSCTRL->DPLLCTRLA.bit.ONDEMAND = 1;
#endif
}

static void sam0_osc_wait_fixed_ready(void)
{
#if DT_ATMEL_SAM0_CLOCK_OSC32K_CLOCK_FREQUENCY
	while (!SYSCTRL->PCLKSR.bit.OSC32KRDY) {
	}
#endif
#if DT_ATMEL_SAM0_CLOCK_XOSC_CLOCK_FREQUENCY
	while (!SYSCTRL->PCLKSR.bit.XOSCRDY) {
	}
#endif
#if DT_ATMEL_SAM0_CLOCK_XOSC32K_CLOCK_FREQUENCY
	while (!SYSCTRL->PCLKSR.bit.XOSC32KRDY) {
	}
#endif
}

static void sam0_osc_wait_programmable_ready(void)
{
#if DT_ATMEL_SAM0_CLOCK_DFLL_CLOCK_FREQUENCY
#ifdef DT_ATMEL_SAM0_CLOCK_DFLL_CLOCK_CONTROLLER
	while (!SYSCTRL->PCLKSR.bit.DFLLLCKC || !SYSCTRL->PCLKSR.bit.DFLLLCKF) {
	}
#endif

	while (!SYSCTRL->PCLKSR.bit.DFLLRDY) {
	}
#endif
#if DT_ATMEL_SAM0_CLOCK_FDPLL_CLOCK_FREQUENCY
	while (!SYSCTRL->DPLLSTATUS.bit.LOCK ||
		!SYSCTRL->DPLLSTATUS.bit.CLKRDY) {
	}
#endif
}

static inline void sam0_gclk_enable_input(u32_t src_id,
					  clock_control_subsys_t sub_system)
{
	u32_t req = (u32_t)sub_system;

	src_id |= GCLK_GENCTRL_ID(req &
			SOC_ATMEL_SAM0_CLOCK_SUBSYS_GCLK_IN_ID_MASK);
	src_id |= GCLK_GENCTRL_IDC | GCLK_GENCTRL_GENEN;
	if (req & SOC_ATMEL_SAM0_CLOCK_SUBSYS_GCLK_IN_DIVSEL) {
		src_id |= GCLK_GENCTRL_DIVSEL;
	}

	GCLK->GENCTRL.reg = src_id;
	sam0_gclk_wait_gclk_synchronization();
}

static inline void sam0_gclk_disable_input(clock_control_subsys_t sub_system)
{
	u32_t req = (u32_t)sub_system;

	req = GCLK_GENCTRL_ID(req &
			SOC_ATMEL_SAM0_CLOCK_SUBSYS_GCLK_IN_ID_MASK);

	GCLK->GENCTRL.reg = req;
	sam0_gclk_wait_gclk_synchronization();
}


static int sam0_gclk_on(struct device *dev,
			clock_control_subsys_t sub_system)
{
	const struct sam0_gclk_config *const cfg = dev->config->config_info;

	if (cfg->gen_id == 1) {
		if (sam0_clock_is_gclk_input(sub_system)) {
			sam0_gclk_enable_input(GCLK_GENCTRL_SRC_GCLKGEN1,
					sub_system);
			return 0;
		}
	}

#ifdef SOC_ATMEL_SAM0_CLOCK_SUBSYS_TYPE_FDPLL_IN
	if (sam0_clock_is_fdpll_input(sub_system)) {
		GCLK->CLKCTRL.reg = GCLK_CLKCTRL_GEN(cfg->gen_id) |
				GCLK_CLKCTRL_CLKEN |
				GCLK_CLKCTRL_ID_FDPLL;
		sam0_gclk_wait_gclk_synchronization();

		SYSCTRL->DPLLCTRLB.bit.REFCLK =
				SYSCTRL_DPLLCTRLB_REFCLK_GCLK_Val;
		return 0;
	}
#endif

	__ASSERT_NO_MSG(sam0_clock_is_gclk_output(sub_system));
	GCLK->CLKCTRL.reg = GCLK_CLKCTRL_GEN(cfg->gen_id) |
			GCLK_CLKCTRL_CLKEN |
			sam0_clock_control_gclk_output_control(sub_system);
	sam0_gclk_wait_gclk_synchronization();

	return 0;
}

static int sam0_gclk_off(struct device *dev,
			 clock_control_subsys_t sub_system)
{
	const struct sam0_gclk_config *const cfg = dev->config->config_info;

	if (cfg->gen_id == 1) {
		if (sam0_clock_is_gclk_input(sub_system)) {
			sam0_gclk_disable_input(sub_system);
			return 0;
		}
	}

#ifdef SOC_ATMEL_SAM0_CLOCK_SUBSYS_TYPE_FDPLL_IN
	if (sam0_clock_is_fdpll_input(sub_system)) {
		GCLK->CLKCTRL.reg = GCLK_CLKCTRL_GEN(cfg->gen_id) |
				GCLK_CLKCTRL_ID_FDPLL;
		sam0_gclk_wait_gclk_synchronization();
		return 0;
	}
#endif

	__ASSERT_NO_MSG(sam0_clock_is_gclk_output(sub_system));
	GCLK->CLKCTRL.reg = GCLK_CLKCTRL_GEN(cfg->gen_id) |
			sam0_clock_control_gclk_output_control(sub_system);
	sam0_gclk_wait_gclk_synchronization();

	return 0;
}

static int sam0_gclk_get_rate(struct device *dev,
			      clock_control_subsys_t sub_system,
			      u32_t *rate)
{
	ARG_UNUSED(sub_system);

	const struct sam0_gclk_config *const cfg = dev->config->config_info;

	*rate = cfg->clock_frequency;

	return 0;
}

static int sam0_gclk_init(struct device *dev)
{
	const struct sam0_gclk_config *const cfg = dev->config->config_info;
	struct device *input_clock = device_get_binding(cfg->clock_source);
	u32_t input_freq;
	u32_t req = cfg->gen_id;

	req |= SOC_ATMEL_SAM0_CLOCK_SUBSYS_TYPE_GCLK_IN;

	clock_control_get_rate(input_clock, (clock_control_subsys_t)req,
			&input_freq);

	__ASSERT_NO_MSG(input_freq >= cfg->clock_frequency);

	if (input_freq != cfg->clock_frequency) {
		u32_t div = (input_freq + cfg->clock_frequency / 2U) /
				cfg->clock_frequency;

		/* If it's an exact power of two, use DIVSEL */
		if ((div & (div - 1)) == 0) {
			req |= SOC_ATMEL_SAM0_CLOCK_SUBSYS_GCLK_IN_DIVSEL;
			div = (31U - __builtin_clz(div));
		}

		GCLK->GENDIV.reg = GCLK_GENDIV_ID(cfg->gen_id) |
				GCLK_GENDIV_DIV(div - 1U);
		sam0_gclk_wait_gclk_synchronization();
	}

	/* CPU clock setup */
	if (cfg->gen_id == 0) {
		sam0_osc_wait_fixed_ready();
		sam0_osc_wait_programmable_ready();

		sam0_dividers_init(CONFIG_SYS_CLOCK_HW_CYCLES_PER_SEC);
	}

	/*
	 * This may actually be calling an uninitialized device (e.g. a GCLK
	 * connected to a programmable oscillator), but since the request
	 * only manipulates GCLK registers, this still works.
	 */
	clock_control_on(input_clock, (clock_control_subsys_t)req);

	/* All clock setup complete */
	if (cfg->gen_id == 0) {
		sam0_osc_ondemand();
	}

	return 0;
}

static const struct clock_control_driver_api sam0_gclk_api = {
	.on = sam0_gclk_on,
	.off = sam0_gclk_off,
	.get_rate = sam0_gclk_get_rate,
};

#define SAM0_GCLK_DECLARE(n, init_priority)				\
static const struct sam0_gclk_config sam0_gclk_config_##n = {		\
	.clock_frequency = DT_ATMEL_SAM0_CLOCK_GCLK##n##_CLOCK_FREQUENCY,\
	.clock_source = DT_ATMEL_SAM0_CLOCK_GCLK##n##_CLOCK_CONTROLLER,	\
	.gen_id = GCLK_CLKCTRL_GEN_GCLK##n##_Val,			\
};									\
DEVICE_AND_API_INIT(sam0_gclk_##n, DT_ATMEL_SAM0_CLOCK_GCLK##n##_LABEL,	\
		    sam0_gclk_init, NULL,				\
		    &sam0_gclk_config_##n, PRE_KERNEL_1, init_priority,	\
		    &sam0_gclk_api);

/* GCLK is the core clock source, so it's mandatory */
SAM0_GCLK_DECLARE(0, SAM0_CLOCK_INIT_PRIORITY_GCLK_0)

#if DT_ATMEL_SAM0_CLOCK_GCLK1_CLOCK_FREQUENCY
SAM0_GCLK_DECLARE(1, SAM0_CLOCK_INIT_PRIORITY_GCLK_ANY)
#endif

#if DT_ATMEL_SAM0_CLOCK_GCLK2_CLOCK_FREQUENCY
SAM0_GCLK_DECLARE(2, SAM0_CLOCK_INIT_PRIORITY_GCLK_ANY)
#endif

#if DT_ATMEL_SAM0_CLOCK_GCLK3_CLOCK_FREQUENCY
SAM0_GCLK_DECLARE(3, SAM0_CLOCK_INIT_PRIORITY_GCLK_ANY)
#endif

#if DT_ATMEL_SAM0_CLOCK_GCLK4_CLOCK_FREQUENCY
SAM0_GCLK_DECLARE(4, SAM0_CLOCK_INIT_PRIORITY_GCLK_ANY)
#endif

#if DT_ATMEL_SAM0_CLOCK_GCLK5_CLOCK_FREQUENCY
SAM0_GCLK_DECLARE(5, SAM0_CLOCK_INIT_PRIORITY_GCLK_ANY)
#endif

#if DT_ATMEL_SAM0_CLOCK_GCLK6_CLOCK_FREQUENCY
SAM0_GCLK_DECLARE(6, SAM0_CLOCK_INIT_PRIORITY_GCLK_ANY)
#endif

#if DT_ATMEL_SAM0_CLOCK_GCLK7_CLOCK_FREQUENCY
SAM0_GCLK_DECLARE(7, SAM0_CLOCK_INIT_PRIORITY_GCLK_ANY)
#endif

#if DT_ATMEL_SAM0_CLOCK_GCLK8_CLOCK_FREQUENCY
SAM0_GCLK_DECLARE(8, SAM0_CLOCK_INIT_PRIORITY_GCLK_ANY)
#endif


#if DT_ATMEL_SAM0_CLOCK_OSC8M_CLOCK_FREQUENCY
static int sam0_osc8m_on(struct device *dev,
			 clock_control_subsys_t sub_system)
{
	ARG_UNUSED(dev);

	__ASSERT_NO_MSG(sam0_clock_is_gclk_input(sub_system));
	sam0_gclk_enable_input(GCLK_GENCTRL_SRC_OSC8M, sub_system);

	return 0;
}

static int sam0_osc8m_off(struct device *dev,
			  clock_control_subsys_t sub_system)
{
	ARG_UNUSED(dev);

	__ASSERT_NO_MSG(sam0_clock_is_gclk_input(sub_system));
	sam0_gclk_disable_input(sub_system);

	return 0;
}

static int sam0_osc8m_get_rate(struct device *dev,
			       clock_control_subsys_t sub_system,
			       u32_t *rate)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(sub_system);

	*rate = DT_ATMEL_SAM0_CLOCK_OSC8M_CLOCK_FREQUENCY;

	return 0;
}

static int sam0_osc8m_init(struct device *dev)
{
	ARG_UNUSED(dev);

#if DT_ATMEL_SAM0_CLOCK_OSC8M_CLOCK_FREQUENCY == MHZ(8)
	SYSCTRL->OSC8M.bit.PRESC = 0;
#elif DT_ATMEL_SAM0_CLOCK_OSC8M_CLOCK_FREQUENCY == MHZ(4)
	SYSCTRL->OSC8M.bit.PRESC = 1;
#elif DT_ATMEL_SAM0_CLOCK_OSC8M_CLOCK_FREQUENCY == MHZ(2)
	SYSCTRL->OSC8M.bit.PRESC = 2;
#elif DT_ATMEL_SAM0_CLOCK_OSC8M_CLOCK_FREQUENCY == MHZ(1)
	SYSCTRL->OSC8M.bit.PRESC = 3;
#else
#error Unsupported OSC8M prescaler
#endif
	SYSCTRL->OSC8M.bit.ONDEMAND = 0;

	return 0;
}

static const struct clock_control_driver_api sam0_osc8m_api = {
	.on = sam0_osc8m_on,
	.off = sam0_osc8m_off,
	.get_rate = sam0_osc8m_get_rate,
};

DEVICE_AND_API_INIT(sam0_osc8m, DT_ATMEL_SAM0_CLOCK_OSC8M_LABEL,
		    sam0_osc8m_init, NULL, NULL, PRE_KERNEL_1,
		    SAM0_CLOCK_INIT_PRIORITY_FIXED_OSC,
		    &sam0_osc8m_api);
#endif


#if DT_ATMEL_SAM0_CLOCK_OSC32K_CLOCK_FREQUENCY
static int sam0_osc32k_on(struct device *dev,
			 clock_control_subsys_t sub_system)
{
	ARG_UNUSED(dev);

	__ASSERT_NO_MSG(sam0_clock_is_gclk_input(sub_system));
	sam0_gclk_enable_input(GCLK_GENCTRL_SRC_OSC32K, sub_system);

	return 0;
}

static int sam0_osc32k_off(struct device *dev,
			  clock_control_subsys_t sub_system)
{
	ARG_UNUSED(dev);

	__ASSERT_NO_MSG(sam0_clock_is_gclk_input(sub_system));
	sam0_gclk_disable_input(sub_system);

	return 0;
}

static int sam0_osc32k_get_rate(struct device *dev,
			       clock_control_subsys_t sub_system,
			       u32_t *rate)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(sub_system);

	*rate = DT_ATMEL_SAM0_CLOCK_OSC32K_CLOCK_FREQUENCY;

	return 0;
}

static int sam0_osc32k_init(struct device *dev)
{
	ARG_UNUSED(dev);

#ifdef FUSES_OSC32K_CAL_ADDR
	u32_t cal = ((*((u32_t *) FUSES_OSC32K_CAL_ADDR)) &
		FUSES_OSC32K_CAL_Msk) >> FUSES_OSC32K_CAL_Pos;
#else
	u32_t cal = ((*((u32_t *) FUSES_OSC32KCAL_ADDR)) &
		FUSES_OSC32KCAL_Msk) >> FUSES_OSC32KCAL_Pos;
#endif
	SYSCTRL->OSC32K.reg = SYSCTRL_OSC32K_CALIB(cal) |
			      SYSCTRL_OSC32K_STARTUP(6) |
			      SYSCTRL_OSC32K_EN32K |
			      SYSCTRL_OSC32K_ENABLE;

	return 0;
}

static const struct clock_control_driver_api sam0_osc32k_api = {
	.on = sam0_osc32k_on,
	.off = sam0_osc32k_off,
	.get_rate = sam0_osc32k_get_rate,
};

DEVICE_AND_API_INIT(sam0_osc32k, DT_ATMEL_SAM0_CLOCK_OSC32K_LABEL,
		    sam0_osc32k_init, NULL, NULL, PRE_KERNEL_1,
		    SAM0_CLOCK_INIT_PRIORITY_FIXED_OSC,
		    &sam0_osc32k_api);
#endif


#if DT_ATMEL_SAM0_CLOCK_ULPOSC32K_CLOCK_FREQUENCY
static int sam0_ulposc32k_on(struct device *dev,
			 clock_control_subsys_t sub_system)
{
	ARG_UNUSED(dev);

	__ASSERT_NO_MSG(sam0_clock_is_gclk_input(sub_system));
	sam0_gclk_enable_input(GCLK_GENCTRL_SRC_OSCULP32K, sub_system);

	return 0;
}

static int sam0_ulposc32k_off(struct device *dev,
			  clock_control_subsys_t sub_system)
{
	ARG_UNUSED(dev);

	__ASSERT_NO_MSG(sam0_clock_is_gclk_input(sub_system));
	sam0_gclk_disable_input(sub_system);

	return 0;
}

static int sam0_ulposc32k_get_rate(struct device *dev,
			       clock_control_subsys_t sub_system,
			       u32_t *rate)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(sub_system);

	*rate = DT_ATMEL_SAM0_CLOCK_ULPOSC32K_CLOCK_FREQUENCY;

	return 0;
}

static int sam0_ulposc32k_init(struct device *dev)
{
	ARG_UNUSED(dev);

	return 0;
}

static const struct clock_control_driver_api sam0_ulposc32k_api = {
	.on = sam0_ulposc32k_on,
	.off = sam0_ulposc32k_off,
	.get_rate = sam0_ulposc32k_get_rate,
};

DEVICE_AND_API_INIT(sam0_ulposc32k, DT_ATMEL_SAM0_CLOCK_ULPOSC32K_LABEL,
		    sam0_ulposc32k_init, NULL, NULL, PRE_KERNEL_1,
		    SAM0_CLOCK_INIT_PRIORITY_FIXED_OSC,
		    &sam0_ulposc32k_api);
#endif


#if DT_ATMEL_SAM0_CLOCK_XOSC32K_CLOCK_FREQUENCY
static int sam0_xosc32k_on(struct device *dev,
			 clock_control_subsys_t sub_system)
{
	ARG_UNUSED(dev);

#ifdef SOC_ATMEL_SAM0_CLOCK_SUBSYS_TYPE_FDPLL_IN
	if (sam0_clock_is_fdpll_input(sub_system)) {
		SYSCTRL->DPLLCTRLB.bit.REFCLK =
			SYSCTRL_DPLLCTRLB_REFCLK_REF0_Val;
		return 0;
	}
#endif

	__ASSERT_NO_MSG(sam0_clock_is_gclk_input(sub_system));
	sam0_gclk_enable_input(GCLK_GENCTRL_SRC_XOSC32K, sub_system);

	return 0;
}

static int sam0_xosc32k_off(struct device *dev,
			  clock_control_subsys_t sub_system)
{
	ARG_UNUSED(dev);

	__ASSERT_NO_MSG(sam0_clock_is_gclk_input(sub_system));
	sam0_gclk_disable_input(sub_system);

	return 0;
}

static int sam0_xosc32k_get_rate(struct device *dev,
			       clock_control_subsys_t sub_system,
			       u32_t *rate)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(sub_system);

	*rate = DT_ATMEL_SAM0_CLOCK_XOSC32K_CLOCK_FREQUENCY;

	return 0;
}

static int sam0_xosc32k_init(struct device *dev)
{
	ARG_UNUSED(dev);

	SYSCTRL->XOSC32K.reg = SYSCTRL_XOSC32K_STARTUP(6) |
			       SYSCTRL_XOSC32K_XTALEN |
			       SYSCTRL_XOSC32K_EN32K |
			       SYSCTRL_XOSC32K_ENABLE;

	return 0;
}

static const struct clock_control_driver_api sam0_xosc32k_api = {
	.on = sam0_xosc32k_on,
	.off = sam0_xosc32k_off,
	.get_rate = sam0_xosc32k_get_rate,
};

DEVICE_AND_API_INIT(sam0_xosc32k, DT_ATMEL_SAM0_CLOCK_XOSC32K_LABEL,
		    sam0_xosc32k_init, NULL, NULL, PRE_KERNEL_1,
		    SAM0_CLOCK_INIT_PRIORITY_FIXED_OSC,
		    &sam0_xosc32k_api);
#endif


#if DT_ATMEL_SAM0_CLOCK_XOSC_CLOCK_FREQUENCY
static int sam0_xosc_on(struct device *dev,
			 clock_control_subsys_t sub_system)
{
	ARG_UNUSED(dev);

#ifdef SOC_ATMEL_SAM0_CLOCK_SUBSYS_TYPE_FDPLL_IN
	if (sam0_clock_is_fdpll_input(sub_system)) {
		SYSCTRL->DPLLCTRLB.bit.REFCLK =
			SYSCTRL_DPLLCTRLB_REFCLK_REF1_Val;
		return 0;
	}
#endif

	__ASSERT_NO_MSG(sam0_clock_is_gclk_input(sub_system));
	sam0_gclk_enable_input(GCLK_GENCTRL_SRC_XOSC, sub_system);

	return 0;
}

static int sam0_xosc_off(struct device *dev,
			  clock_control_subsys_t sub_system)
{
	ARG_UNUSED(dev);

	__ASSERT_NO_MSG(sam0_clock_is_gclk_input(sub_system));
	sam0_gclk_disable_input(sub_system);

	return 0;
}

static int sam0_xosc_get_rate(struct device *dev,
			       clock_control_subsys_t sub_system,
			       u32_t *rate)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(sub_system);

	*rate = DT_ATMEL_SAM0_CLOCK_XOSC_CLOCK_FREQUENCY;

	return 0;
}

static int sam0_xosc_init(struct device *dev)
{
	ARG_UNUSED(dev);

	u32_t reg = SYSCTRL_XOSC_XTALEN |
		    SYSCTRL_XOSC_ENABLE;

	/* SAMD21 Manual 17.8.5 (XOSC register description) */
	if (DT_ATMEL_SAM0_CLOCK_XOSC_CLOCK_FREQUENCY <= MHZ(2)) {
		reg |= SYSCTRL_XOSC_GAIN(0);
	} else if (DT_ATMEL_SAM0_CLOCK_XOSC_CLOCK_FREQUENCY <= MHZ(4)) {
		reg |= SYSCTRL_XOSC_GAIN(1);
	} else if (DT_ATMEL_SAM0_CLOCK_XOSC_CLOCK_FREQUENCY <= MHZ(8)) {
		reg |= SYSCTRL_XOSC_GAIN(2);
	} else if (DT_ATMEL_SAM0_CLOCK_XOSC_CLOCK_FREQUENCY <= MHZ(16)) {
		reg |= SYSCTRL_XOSC_GAIN(3);
	} else {
		reg |= SYSCTRL_XOSC_GAIN(4);
	}

	/*
	 * SAMD21 Manual 37.13.1.2 (Crystal oscillator characteristics,
	 * t_startup).
	 */
	if (DT_ATMEL_SAM0_CLOCK_XOSC_CLOCK_FREQUENCY <= MHZ(2)) {
		/* 48k cycles @ 2MHz */
		reg |= SYSCTRL_XOSC_STARTUP(7);
	} else if (DT_ATMEL_SAM0_CLOCK_XOSC_CLOCK_FREQUENCY <= MHZ(4)) {
		/* 20k cycles @ 4MHz */
		reg |= SYSCTRL_XOSC_STARTUP(8);
	} else if (DT_ATMEL_SAM0_CLOCK_XOSC_CLOCK_FREQUENCY <= MHZ(8)) {
		/* 13k cycles @ 8MHz */
		reg |= SYSCTRL_XOSC_STARTUP(6);
	} else if (DT_ATMEL_SAM0_CLOCK_XOSC_CLOCK_FREQUENCY <= MHZ(16)) {
		/* 15k cycles @ 16MHz */
		reg |= SYSCTRL_XOSC_STARTUP(5);
	} else {
		/* 10k cycles @ 32MHz */
		reg |= SYSCTRL_XOSC_STARTUP(4);
	}

	SYSCTRL->XOSC.reg = reg;

	return 0;
}

static const struct clock_control_driver_api sam0_xosc_api = {
	.on = sam0_xosc_on,
	.off = sam0_xosc_off,
	.get_rate = sam0_xosc_get_rate,
};

DEVICE_AND_API_INIT(sam0_xosc, DT_ATMEL_SAM0_CLOCK_XOSC_LABEL,
		    sam0_xosc_init, NULL, NULL, PRE_KERNEL_1,
		    SAM0_CLOCK_INIT_PRIORITY_FIXED_OSC,
		    &sam0_xosc_api);
#endif


#if DT_ATMEL_SAM0_CLOCK_DFLL_CLOCK_FREQUENCY
static int sam0_dfll_on(struct device *dev,
			 clock_control_subsys_t sub_system)
{
	ARG_UNUSED(dev);

	__ASSERT_NO_MSG(sam0_clock_is_gclk_input(sub_system));
	sam0_gclk_enable_input(GCLK_GENCTRL_SRC_DFLL48M, sub_system);

	return 0;
}

static int sam0_dfll_off(struct device *dev,
			  clock_control_subsys_t sub_system)
{
	ARG_UNUSED(dev);

	__ASSERT_NO_MSG(sam0_clock_is_gclk_input(sub_system));
	sam0_gclk_disable_input(sub_system);

	return 0;
}

static int sam0_dfll_get_rate(struct device *dev,
			       clock_control_subsys_t sub_system,
			       u32_t *rate)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(sub_system);

	*rate = DT_ATMEL_SAM0_CLOCK_DFLL_CLOCK_FREQUENCY;

	return 0;
}

static int sam0_dfll_init(struct device *dev)
{
	ARG_UNUSED(dev);

	sam0_osc_wait_fixed_ready();

	/*
	 * Errata 1.2.1: the DFLL will freeze the device if is is changed
	 * while in ONDEMAND mode (the default) with no clock requested. So
	 * enable it without ONDEMAND set before doing anything else.
	 */
	SYSCTRL->DFLLCTRL.reg = SYSCTRL_DFLLCTRL_ENABLE;

	/* Load the 48MHz calibration */
	u32_t coarse = ((*((u32_t *)FUSES_DFLL48M_COARSE_CAL_ADDR)) &
		FUSES_DFLL48M_COARSE_CAL_Msk) >> FUSES_DFLL48M_COARSE_CAL_Pos;
	u32_t fine = ((*((u32_t *)FUSES_DFLL48M_FINE_CAL_ADDR)) &
		FUSES_DFLL48M_FINE_CAL_Msk) >> FUSES_DFLL48M_FINE_CAL_Pos;

	SYSCTRL->DFLLVAL.reg = SYSCTRL_DFLLVAL_COARSE(coarse) |
			       SYSCTRL_DFLLVAL_FINE(fine);

	u32_t reg = SYSCTRL_DFLLCTRL_QLDIS |
#ifdef SYSCTRL_DFLLCTRL_WAITLOCK
			SYSCTRL_DFLLCTRL_WAITLOCK |
#endif
			SYSCTRL_DFLLCTRL_ENABLE;


#ifdef DT_ATMEL_SAM0_CLOCK_DFLL_CLOCK_CONTROLLER
	/* Use closed loop mode if we have a reference clock */
	struct device *input_clock = device_get_binding(
		DT_ATMEL_SAM0_CLOCK_DFLL_CLOCK_CONTROLLER);
	u32_t input_freq;
	/*
	 * ASF doesn't always define the constant for the DFLL ID,
	 * but it's zero).
	 */
	clock_control_subsys_t req = SOC_ATMEL_SAM0_GCLK_SUBSYS(0);

	clock_control_get_rate(input_clock, req, &input_freq);
	__ASSERT_NO_MSG(input_freq <= DT_ATMEL_SAM0_CLOCK_DFLL_CLOCK_FREQUENCY);
	clock_control_on(input_clock, req);

	reg |= SYSCTRL_DFLLCTRL_MODE;

	u32_t mul = (DT_ATMEL_SAM0_CLOCK_DFLL_CLOCK_FREQUENCY +
			input_freq / 2U) / input_freq;

	/* Steps are half the maximum value */
	SYSCTRL->DFLLMUL.reg = SYSCTRL_DFLLMUL_CSTEP(31) |
			       SYSCTRL_DFLLMUL_FSTEP(511) |
			       SYSCTRL_DFLLMUL_MUL(mul);
#endif

	SYSCTRL->DFLLCTRL.reg = reg;

	return 0;
}

static const struct clock_control_driver_api sam0_dfll_api = {
	.on = sam0_dfll_on,
	.off = sam0_dfll_off,
	.get_rate = sam0_dfll_get_rate,
};

DEVICE_AND_API_INIT(sam0_dfll, DT_ATMEL_SAM0_CLOCK_DFLL_LABEL,
		    sam0_dfll_init, NULL, NULL, PRE_KERNEL_1,
		    SAM0_CLOCK_INIT_PRIORITY_PROG_OSC,
		    &sam0_dfll_api);
#endif


#if DT_ATMEL_SAM0_CLOCK_FDPLL_CLOCK_FREQUENCY
static int sam0_fdpll_on(struct device *dev,
			 clock_control_subsys_t sub_system)
{
	ARG_UNUSED(dev);

	__ASSERT_NO_MSG(sam0_clock_is_gclk_input(sub_system));
	sam0_gclk_enable_input(GCLK_GENCTRL_SRC_FDPLL, sub_system);

	return 0;
}

static int sam0_fdpll_off(struct device *dev,
			  clock_control_subsys_t sub_system)
{
	ARG_UNUSED(dev);

	__ASSERT_NO_MSG(sam0_clock_is_gclk_input(sub_system));
	sam0_gclk_disable_input(sub_system);

	return 0;
}

static int sam0_fdpll_get_rate(struct device *dev,
			       clock_control_subsys_t sub_system,
			       u32_t *rate)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(sub_system);

	*rate = DT_ATMEL_SAM0_CLOCK_FDPLL_CLOCK_FREQUENCY;

	return 0;
}

static int sam0_fdpll_init(struct device *dev)
{
	ARG_UNUSED(dev);

	struct device *input_clock = device_get_binding(
		DT_ATMEL_SAM0_CLOCK_FDPLL_CLOCK_CONTROLLER);
	u32_t clk_ref;
	clock_control_subsys_t req = (clock_control_subsys_t)
			SOC_ATMEL_SAM0_CLOCK_SUBSYS_TYPE_FDPLL_IN;
	u32_t reg;

	sam0_osc_wait_fixed_ready();

	clock_control_get_rate(input_clock, req, &clk_ref);
	clock_control_on(input_clock, req);

	if (SYSCTRL->DPLLCTRLB.bit.REFCLK ==
			SYSCTRL_DPLLCTRLB_REFCLK_REF1_Val) {
		/*
		 * Pick the largest possible divisor for XOSC that gives
		 * LDR less than its maximum.  This minimizes the impact of
		 * the fractional part for the highest accuracy.  This also
		 * ensures that the input frequency is less than the limit
		 * (2MHz).
		 */
		u32_t div = clk_ref;
		div /= (2 * DT_ATMEL_SAM0_CLOCK_FDPLL_CLOCK_FREQUENCY);
		div *= 0x1000U;

		/* Ensure it's over the minium frequency */
		div = MIN(div, clk_ref / (2U * KHZ(32)) - 1U);

		if (div > 0x800U) {
			div = 0x800U;
		} else if (div < 1U) {
			div = 1U;
		}

		clk_ref /= (2U * div);

		SYSCTRL->DPLLCTRLB.bit.DIV = div - 1U;
	}

	/*
	 * SAMD21 Manual, 36.13.7: Fractional Digital Phase Locked Loop
	 * (FDPLL96M) Characteristics
	 */
	__ASSERT_NO_MSG(clk_ref >= KHZ(32));
	__ASSERT_NO_MSG(clk_ref <= MHZ(2));
	BUILD_ASSERT(DT_ATMEL_SAM0_CLOCK_FDPLL_CLOCK_FREQUENCY >= MHZ(48));
	BUILD_ASSERT(DT_ATMEL_SAM0_CLOCK_FDPLL_CLOCK_FREQUENCY <= MHZ(96));
	__ASSERT_NO_MSG(clk_ref <= DT_ATMEL_SAM0_CLOCK_FDPLL_CLOCK_FREQUENCY);

	u32_t mul = DT_ATMEL_SAM0_CLOCK_FDPLL_CLOCK_FREQUENCY;

	mul <<= 4U;
	mul += clk_ref / 2U;
	mul /= clk_ref;

	u32_t ldr = mul >> 4U;
	u32_t ldrfrac = mul & 0xFU;

	SYSCTRL->DPLLRATIO.reg = SYSCTRL_DPLLRATIO_LDR(ldr - 1U) |
			SYSCTRL_DPLLRATIO_LDRFRAC(ldrfrac);

	SYSCTRL->DPLLCTRLA.reg = SYSCTRL_DPLLCTRLA_ENABLE;

	return 0;
}

static const struct clock_control_driver_api sam0_fdpll_api = {
	.on = sam0_fdpll_on,
	.off = sam0_fdpll_off,
	.get_rate = sam0_fdpll_get_rate,
};

DEVICE_AND_API_INIT(sam0_fdpll, DT_ATMEL_SAM0_CLOCK_FDPLL_LABEL,
		    sam0_fdpll_init, NULL, NULL, PRE_KERNEL_1,
		    SAM0_CLOCK_INIT_PRIORITY_PROG_OSC,
		    &sam0_fdpll_api);
#endif
