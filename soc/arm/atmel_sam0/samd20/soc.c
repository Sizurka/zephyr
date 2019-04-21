/*
 * Copyright (c) 2018 Sean Nyekjaer
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Atmel SAMD MCU series initialization code
 */

#include <arch/cpu.h>
#include <cortex_m/exc.h>
#include <device.h>
#include <init.h>
#include <kernel.h>
#include <soc.h>

void soc_sam0_clock_init(void);

static int atmel_samd_init(struct device *arg)
{
	u32_t key;

	ARG_UNUSED(arg);

	key = irq_lock();

	z_clearfaults();

	soc_sam0_clock_init();

	/* Install default handler that simply resets the CPU
	 * if configured in the kernel, NOP otherwise
	 */
	NMI_INIT();

	irq_unlock(key);

	return 0;
}

SYS_INIT(atmel_samd_init, PRE_KERNEL_1, 0);
