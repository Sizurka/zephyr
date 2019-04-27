/*
 * Copyright (c) 2017 Google LLC.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _ATMEL_SAMD_SOC_H_
#define _ATMEL_SAMD_SOC_H_

#ifndef _ASMLANGUAGE

#define DONT_USE_CMSIS_INIT

#include <zephyr/types.h>

#if defined(CONFIG_SOC_PART_NUMBER_SAMD21E15A)
#include <samd21e15a.h>
#elif defined(CONFIG_SOC_PART_NUMBER_SAMD21E16A)
#include <samd21e16a.h>
#elif defined(CONFIG_SOC_PART_NUMBER_SAMD21E17A)
#include <samd21e17a.h>
#elif defined(CONFIG_SOC_PART_NUMBER_SAMD21E18A)
#include <samd21e18a.h>
#elif defined(CONFIG_SOC_PART_NUMBER_SAMD21G15A)
#include <samd21g15a.h>
#elif defined(CONFIG_SOC_PART_NUMBER_SAMD21G16A)
#include <samd21g16a.h>
#elif defined(CONFIG_SOC_PART_NUMBER_SAMD21G17A)
#include <samd21g17a.h>
#elif defined(CONFIG_SOC_PART_NUMBER_SAMD21G17AU)
#include <samd21g17au.h>
#elif defined(CONFIG_SOC_PART_NUMBER_SAMD21G18A)
#include <samd21g18a.h>
#elif defined(CONFIG_SOC_PART_NUMBER_SAMD21G18AU)
#include <samd21g18au.h>
#elif defined(CONFIG_SOC_PART_NUMBER_SAMD21J15A)
#include <samd21j15a.h>
#elif defined(CONFIG_SOC_PART_NUMBER_SAMD21J16A)
#include <samd21j16a.h>
#elif defined(CONFIG_SOC_PART_NUMBER_SAMD21J17A)
#include <samd21j17a.h>
#elif defined(CONFIG_SOC_PART_NUMBER_SAMD21J18A)
#include <samd21j18a.h>
#else
#error Library does not support the specified device.
#endif

#endif /* _ASMLANGUAGE */

#define SOC_ATMEL_SAM0_CLOCK_SUBSYS_TYPE_MASK		0xFF000000U
#define SOC_ATMEL_SAM0_CLOCK_SUBSYS_TYPE_GCLK_OUT	0x01000000U

#define SOC_ATMEL_SAM0_CLOCK_SUBSYS_GCLK_OUT_ID_MASK	0x0000FFFFU
#define SOC_ATMEL_SAM0_GCLK_SUBSYS(gclk_id) \
	(clock_control_subsys_t)(SOC_ATMEL_SAM0_CLOCK_SUBSYS_TYPE_GCLK_OUT | \
	gclk_id)

#endif /* _ATMEL_SAMD_SOC_H_ */
