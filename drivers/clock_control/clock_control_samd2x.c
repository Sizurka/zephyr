/*
 * Copyright (c) 2019 Derek Hageman <hageman@inthat.cloud>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <soc.h>
#include <clock_control.h>

#define SAM0_CLOCK_INIT_PRIORITY_GCLK_0		3
#define SAM0_CLOCK_INIT_PRIORITY_GCLK_ANY	1


BUILD_ASSERT(CONFIG_SYSTEM_CLOCK_INIT_PRIORITY > \
	     SAM0_CLOCK_INIT_PRIORITY_GCLK_0);

struct sam0_gclk_config {
	u32_t clock_frequency;
	u8_t gen_id;
};


static int sam0_gclk_on(struct device *dev,
			clock_control_subsys_t sub_system)
{
	const struct sam0_gclk_config *const cfg = dev->config->config_info;
	u32_t req = (u32_t)sub_system;

	__ASSERT_NO_MSG((req & SOC_ATMEL_SAM0_CLOCK_SUBSYS_TYPE_MASK) == \
		SOC_ATMEL_SAM0_CLOCK_SUBSYS_GCLK_OUT);

	req &= SOC_ATMEL_SAM0_CLOCK_SUBSYS_GCLK_OUT_ID_MASK;

	GCLK->CLKCTRL.reg = GCLK_CLKCTRL_GEN(cfg->gen_id) |
			GCLK_CLKCTRL_CLKEN |
			GCLK_CLKCTRL_ID(req);

	return 0;
}

static int sam0_gclk_off(struct device *dev,
			 clock_control_subsys_t sub_system)
{
	const struct sam0_gclk_config *const cfg = dev->config->config_info;
	u32_t req = (u32_t)sub_system;

	__ASSERT_NO_MSG((req & SOC_ATMEL_SAM0_CLOCK_SUBSYS_TYPE_MASK) == \
		SOC_ATMEL_SAM0_CLOCK_SUBSYS_GCLK_OUT);

	req &= SOC_ATMEL_SAM0_CLOCK_SUBSYS_GCLK_OUT_ID_MASK;

	GCLK->CLKCTRL.reg = GCLK_CLKCTRL_GEN(cfg->gen_id) |
			GCLK_CLKCTRL_ID(req);

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
	.gen_id = GCLK_CLKCTRL_GEN_GCLK##n##_Val,			\
};									\
DEVICE_AND_API_INIT(sam0_gclk_##n, DT_ATMEL_SAM0_CLOCK_GCLK##n##_LABEL,	\
		    sam0_gclk_init, NULL,				\
		    &sam0_gclk_config_##n, PRE_KERNEL_1, init_priority,	\
		    &sam0_gclk_api);

#if DT_ATMEL_SAM0_CLOCK_GCLK0_CLOCK_FREQUENCY
SAM0_GCLK_DECLARE(0, SAM0_CLOCK_INIT_PRIORITY_GCLK_0)
#endif

#if DT_ATMEL_SAM0_CLOCK_GCLK1_CLOCK_FREQUENCY
SAM0_GCLK_DECLARE(1, SAM0_CLOCK_INIT_PRIORITY_GCLK_ANY)
#endif

#if DT_ATMEL_SAM0_CLOCK_GCLK2_CLOCK_FREQUENCY
SAM0_GCLK_DECLARE(2, SAM0_CLOCK_INIT_PRIORITY_GCLK_ANY)
#endif

#if DT_ATMEL_SAM0_CLOCK_GCLK3_CLOCK_FREQUENCY
SAM0_GCLK_DECLARE(3, SAM0_CLOCK_INIT_PRIORITY_GCLK_ANY)
#endif