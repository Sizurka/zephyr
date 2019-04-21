/*
 * Copyright (c) 2019 Derek Hageman <hageman@inthat.cloud>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <soc.h>
#include <device.h>

static void soc_sam0_flash_wait_init(u32_t ahb_frequency)
{
	/* SAMD21 Manual: 37.12 wait states for Vdd >= 2.7V */
	if (ahb_frequency <= 24000000) {
		NVMCTRL->CTRLB.bit.RWS = 0;
	} else {
		NVMCTRL->CTRLB.bit.RWS = 1;
	}
}

static void soc_sam0_wait_gclk_synchronization(void)
{
	while (GCLK->STATUS.bit.SYNCBUSY) {
	}
}

static void soc_sam0_osc8m_init(void)
{
#if DT_ATMEL_SAM0_CLK_SRC_OSC8M_CLOCK_FREQUENCY == MHZ(8)
	SYSCTRL->OSC8M.bit.PRESC = 0;
#elif DT_ATMEL_SAM0_CLK_SRC_OSC8M_CLOCK_FREQUENCY == MHZ(4)
	SYSCTRL->OSC8M.bit.PRESC = 1;
#elif DT_ATMEL_SAM0_CLK_SRC_OSC8M_CLOCK_FREQUENCY == MHZ(2)
	SYSCTRL->OSC8M.bit.PRESC = 2;
#elif DT_ATMEL_SAM0_CLK_SRC_OSC8M_CLOCK_FREQUENCY == MHZ(1)
	SYSCTRL->OSC8M.bit.PRESC = 3;
#else
#error Unsupported OSC8M prescaler
#endif
	SYSCTRL->OSC8M.bit.ONDEMAND = 0;
}

static void soc_sam0_osc32k_init(void)
{
#if DT_ATMEL_SAM0_CLK_SRC_OSC32K_CLOCK_FREQUENCY
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
#endif
}

static void soc_sam0_xosc_init(void)
{
#if DT_ATMEL_SAM0_CLK_SRC_XOSC_CLOCK_FREQUENCY
	u32_t reg = SYSCTRL_XOSC_XTALEN |
		    SYSCTRL_XOSC_ENABLE;

	/* SAMD21 Manual 17.8.5 (XOSC register description) */
	if (DT_ATMEL_SAM0_CLK_SRC_XOSC_CLOCK_FREQUENCY <= MHZ(2)) {
		reg |= SYSCTRL_XOSC_GAIN(0);
	} else if (DT_ATMEL_SAM0_CLK_SRC_XOSC_CLOCK_FREQUENCY <= MHZ(4)) {
		reg |= SYSCTRL_XOSC_GAIN(1);
	} else if (DT_ATMEL_SAM0_CLK_SRC_XOSC_CLOCK_FREQUENCY <= MHZ(8)) {
		reg |= SYSCTRL_XOSC_GAIN(2);
	} else if (DT_ATMEL_SAM0_CLK_SRC_XOSC_CLOCK_FREQUENCY <= MHZ(16)) {
		reg |= SYSCTRL_XOSC_GAIN(3);
	} else {
		reg |= SYSCTRL_XOSC_GAIN(4);
	}

	/*
	 * SAMD21 Manual 37.13.1.2 (Crystal oscillator characteristics,
	 * t_startup).
	 */
	if (DT_ATMEL_SAM0_CLK_SRC_XOSC_CLOCK_FREQUENCY <= MHZ(2)) {
		/* 48k cycles @ 2MHz */
		reg |= SYSCTRL_XOSC_STARTUP(7);
	} else if (DT_ATMEL_SAM0_CLK_SRC_XOSC_CLOCK_FREQUENCY <= MHZ(4)) {
		/* 20k cycles @ 4MHz */
		reg |= SYSCTRL_XOSC_STARTUP(8);
	} else if (DT_ATMEL_SAM0_CLK_SRC_XOSC_CLOCK_FREQUENCY <= MHZ(8)) {
		/* 13k cycles @ 8MHz */
		reg |= SYSCTRL_XOSC_STARTUP(6);
	} else if (DT_ATMEL_SAM0_CLK_SRC_XOSC_CLOCK_FREQUENCY <= MHZ(16)) {
		/* 15k cycles @ 16MHz */
		reg |= SYSCTRL_XOSC_STARTUP(5);
	} else {
		/* 10k cycles @ 32MHz */
		reg |= SYSCTRL_XOSC_STARTUP(4);
	}

	SYSCTRL->XOSC.reg = reg;
#endif
}

static void soc_sam0_xosc32k_init(void)
{
#if DT_ATMEL_SAM0_CLK_SRC_XOSC32K_CLOCK_FREQUENCY
	SYSCTRL->XOSC32K.reg = SYSCTRL_XOSC32K_STARTUP(6) |
			       SYSCTRL_XOSC32K_XTALEN |
			       SYSCTRL_XOSC32K_EN32K |
			       SYSCTRL_XOSC32K_ENABLE;
#endif
}

static void soc_sam0_wait_fixed_osc_ready(void)
{
#if DT_ATMEL_SAM0_CLK_SRC_OSC32K_CLOCK_FREQUENCY
	while (!SYSCTRL->PCLKSR.bit.OSC32KRDY) {
	}
#endif
#if DT_ATMEL_SAM0_CLK_SRC_XOSC_CLOCK_FREQUENCY
	while (!SYSCTRL->PCLKSR.bit.XOSCRDY) {
	}
#endif
#if DT_ATMEL_SAM0_CLK_SRC_XOSC32K_CLOCK_FREQUENCY
	while (!SYSCTRL->PCLKSR.bit.XOSC32KRDY) {
	}
#endif
}

static void soc_sam0_configure_gclk(u32_t gclkid, u32_t src,
				    u32_t src_frequency, u32_t output_frequency)
{
	u32_t reg = GCLK_GENCTRL_ID(gclkid) |
			GCLK_GENCTRL_SRC(src) |
			GCLK_GENCTRL_IDC |
			GCLK_GENCTRL_GENEN;

	if (src_frequency != output_frequency) {
		u32_t div = (src_frequency + output_frequency / 2U) /
				output_frequency;

		/* If it's an exact power of two, use DIVSEL */
		if ((div & (div - 1)) == 0) {
			reg |= GCLK_GENCTRL_DIVSEL;
			div = (31U - __builtin_clz(div));
		}

		GCLK->GENDIV.reg = GCLK_GENDIV_ID(gclkid) |
				   GCLK_GENDIV_DIV(div - 1U);
		soc_sam0_wait_gclk_synchronization();
	}

	GCLK->GENCTRL.reg = reg;
	soc_sam0_wait_gclk_synchronization();
}

#define SOC_SAM0_SETUP_GCLK(id)						\
	soc_sam0_configure_gclk(id,					\
		DT_ATMEL_SAM0_GCLK_##id##_CLOCK_0_GCLK_SRC_ID,		\
		DT_ATMEL_SAM0_GCLK_##id##_CLOCK_0_CLOCK_FREQUENCY,	\
		DT_ATMEL_SAM0_GCLK_##id##_CLOCK_FREQUENCY)

static void soc_sam0_fdpll_init(void)
{
#if DT_ATMEL_SAM0_FDPLL_0_CLOCK_FREQUENCY
	BUILD_ASSERT(DT_ATMEL_SAM0_FDPLL_0_CLOCK_FREQUENCY >=
		DT_ATMEL_SAM0_FDPLL_0_CLOCK_0_CLOCK_FREQUENCY);

	/*
	 * SAMD21 Manual, 36.13.7: Fractional Digital Phase Locked Loop
	 * (FDPLL96M) Characteristics
	 */
	BUILD_ASSERT(DT_ATMEL_SAM0_FDPLL_0_CLOCK_0_CLOCK_FREQUENCY >= KHZ(32));
	BUILD_ASSERT(DT_ATMEL_SAM0_FDPLL_0_CLOCK_0_FDPLL_SRC_ID ==
			SYSCTRL_DPLLCTRLB_REFCLK_REF1_Val ||
			DT_ATMEL_SAM0_FDPLL_0_CLOCK_0_CLOCK_FREQUENCY <=
			MHZ(2));
	BUILD_ASSERT(DT_ATMEL_SAM0_FDPLL_0_CLOCK_FREQUENCY >= MHZ(48));
	BUILD_ASSERT(DT_ATMEL_SAM0_FDPLL_0_CLOCK_FREQUENCY <= MHZ(96));

	u32_t reg = SYSCTRL_DPLLCTRLB_REFCLK(
			DT_ATMEL_SAM0_FDPLL_0_CLOCK_0_FDPLL_SRC_ID);

	if (DT_ATMEL_SAM0_FDPLL_0_CLOCK_0_FDPLL_SRC_ID ==
			SYSCTRL_DPLLCTRLB_REFCLK_GCLK_Val) {
		/* Route the selected clock to the FDPLL */
		SOC_ATMEL_SAM0_GCLK_SELECT(GCLK_CLKCTRL_ID_FDPLL,
				DT_ATMEL_SAM0_FDPLL_0_CLOCK_0);
		soc_sam0_wait_gclk_synchronization();
	}
	/*
	 * Don't need GCLK_CLKCTRL_ID_FDPLL32K because we're not using
	 * the lock timeout (but it would be easy to put as CLOCK_1).
	 */


	u32_t clk_ref = DT_ATMEL_SAM0_FDPLL_0_CLOCK_0_CLOCK_FREQUENCY;

	if (DT_ATMEL_SAM0_FDPLL_0_CLOCK_0_FDPLL_SRC_ID ==
			SYSCTRL_DPLLCTRLB_REFCLK_REF1_Val) {
		/*
		 * Pick the largest possible divisor for XOSC that gives
		 * LDR less than its maximum.  This minimizes the impact of
		 * the fractional part for the highest accuracy.  This also
		 * ensures that the input frequency is less than the limit
		 * (2MHz).
		 */
		u32_t div = DT_ATMEL_SAM0_FDPLL_0_CLOCK_0_CLOCK_FREQUENCY;
		div /= (2 * DT_ATMEL_SAM0_FDPLL_0_CLOCK_FREQUENCY);
		div *= 0x1000U;

		/* Ensure it's over the minium frequency */
		div = MIN(div, clk_ref / (2U * KHZ(32)) - 1U);

		if (div > 0x800U) {
			div = 0x800U;
		} else if (div < 1U) {
			div = 1U;
		}

		clk_ref /= (2U * div);

		reg |= SYSCTRL_DPLLCTRLB_DIV(div - 1U);
	}

	u32_t mul = DT_ATMEL_SAM0_FDPLL_0_CLOCK_FREQUENCY;
	mul <<= 4U;
	mul += clk_ref / 2U;
	mul /= clk_ref;

	u32_t ldr = mul >> 4U;
	u32_t ldrfrac = mul & 0xFU;

	SYSCTRL->DPLLRATIO.reg = SYSCTRL_DPLLRATIO_LDR(ldr - 1U) |
			SYSCTRL_DPLLRATIO_LDRFRAC(ldrfrac);

	SYSCTRL->DPLLCTRLB.reg = reg;
	SYSCTRL->DPLLCTRLA.reg = SYSCTRL_DPLLCTRLA_ENABLE;
#endif
}

static void soc_sam0_dfll_init(void)
{
#if DT_ATMEL_SAM0_DFLL_0_CLOCK_FREQUENCY
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


#if DT_ATMEL_SAM0_DFLL_0_CLOCK_0_CLOCK_FREQUENCY
	/* Use closed loop mode if we have a reference clock */
	BUILD_ASSERT(DT_ATMEL_SAM0_DFLL_0_CLOCK_FREQUENCY >=
		DT_ATMEL_SAM0_DFLL_0_CLOCK_0_CLOCK_FREQUENCY);

	/*
	 * Route the selected clock to the DFLL (ASF doesn't always define
	 * the constant for the DFLL ID, but it's zero).
	 */
	SOC_ATMEL_SAM0_GCLK_SELECT(0, DT_ATMEL_SAM0_DFLL_0_CLOCK_0);
	soc_sam0_wait_gclk_synchronization();

	reg |= SYSCTRL_DFLLCTRL_MODE;

	u32_t mul = (DT_ATMEL_SAM0_DFLL_0_CLOCK_FREQUENCY +
			DT_ATMEL_SAM0_DFLL_0_CLOCK_0_CLOCK_FREQUENCY / 2U) /
			DT_ATMEL_SAM0_DFLL_0_CLOCK_0_CLOCK_FREQUENCY;

	/* Steps are half the maximum value */
	SYSCTRL->DFLLMUL.reg = SYSCTRL_DFLLMUL_CSTEP(31) |
			       SYSCTRL_DFLLMUL_FSTEP(511) |
			       SYSCTRL_DFLLMUL_MUL(mul);

#endif

	SYSCTRL->DFLLCTRL.reg = reg;
#endif
}

static void soc_sam0_wait_programmable_osc_ready(void)
{
#if DT_ATMEL_SAM0_DFLL_0_CLOCK_FREQUENCY
#if DT_ATMEL_SAM0_DFLL_0_CLOCK_0_CLOCK_FREQUENCY
	while (!SYSCTRL->PCLKSR.bit.DFLLLCKC || !SYSCTRL->PCLKSR.bit.DFLLLCKF) {
	}
#endif

	while (!SYSCTRL->PCLKSR.bit.DFLLRDY) {
	}
#endif
#if DT_ATMEL_SAM0_FDPLL_0_CLOCK_FREQUENCY
	while (!SYSCTRL->DPLLSTATUS.bit.LOCK ||
		!SYSCTRL->DPLLSTATUS.bit.CLKRDY) {
	}
#endif
}

static void soc_sam0_dividers_init(uint32_t cpu_frequency)
{
	u32_t div = DT_ATMEL_SAM0_GCLK_0_CLOCK_0_CLOCK_FREQUENCY /
			cpu_frequency;

	/* Only powers of two up to 128 are available */
	__ASSERT((div & (div - 1)) == 0, "invalid CPU clock frequency");
	div = 31U - __builtin_clz(div);
	__ASSERT(div <= 7, "invalid CPU clock frequency");

	PM->CPUSEL.bit.CPUDIV = div;
	PM->APBASEL.bit.APBADIV = div;
	PM->APBBSEL.bit.APBBDIV = div;
	PM->APBCSEL.bit.APBCDIV = div;
}

static void soc_sam0_osc_ondemand(void)
{
	/*
	 * Set all enabled oscillators to ONDEMAND, so the unneeded ones
	 * can stop.
	 */
	SYSCTRL->OSC8M.bit.ONDEMAND = 1;

#if DT_ATMEL_SAM0_CLK_SRC_OSC32K_CLOCK_FREQUENCY
	SYSCTRL->OSC32K.bit.ONDEMAND = 1;
#endif
#if DT_ATMEL_SAM0_CLK_SRC_XOSC32K_CLOCK_FREQUENCY
	SYSCTRL->XOSC32K.bit.ONDEMAND = 1;
#endif
#if DT_ATMEL_SAM0_CLK_SRC_XOSC_CLOCK_FREQUENCY
	SYSCTRL->XOSC.bit.ONDEMAND = 1;
#endif
#if DT_ATMEL_SAM0_DFLL_0_CLOCK_FREQUENCY
	SYSCTRL->DFLLCTRL.bit.ONDEMAND = 1;
#endif
#if DT_ATMEL_SAM0_FDPLL_0_CLOCK_0_CLOCK_FREQUENCY
	SYSCTRL->DPLLCTRLA.bit.ONDEMAND = 1;
#endif
}

void soc_sam0_clock_init(void)
{
	soc_sam0_flash_wait_init(CONFIG_SYS_CLOCK_HW_CYCLES_PER_SEC);

	/* Setup fixed oscillators */
	soc_sam0_osc8m_init();
	soc_sam0_osc32k_init();
	soc_sam0_xosc_init();
	soc_sam0_xosc32k_init();
	soc_sam0_wait_fixed_osc_ready();

#if DT_ATMEL_SAM0_GCLK_1_BASE_ADDRESS
	BUILD_ASSERT(DT_ATMEL_SAM0_GCLK_0_CLOCK_FREQUENCY <=
		     DT_ATMEL_SAM0_GCLK_0_CLOCK_0_CLOCK_FREQUENCY);
	SOC_SAM0_SETUP_GCLK(1);
#endif
#if DT_ATMEL_SAM0_GCLK_2_BASE_ADDRESS
	BUILD_ASSERT(DT_ATMEL_SAM0_GCLK_1_CLOCK_FREQUENCY <=
		     DT_ATMEL_SAM0_GCLK_1_CLOCK_0_CLOCK_FREQUENCY);
	SOC_SAM0_SETUP_GCLK(2);
#endif
#if DT_ATMEL_SAM0_GCLK_3_BASE_ADDRESS
	BUILD_ASSERT(DT_ATMEL_SAM0_GCLK_2_CLOCK_FREQUENCY <=
		     DT_ATMEL_SAM0_GCLK_2_CLOCK_0_CLOCK_FREQUENCY);
	SOC_SAM0_SETUP_GCLK(3);
#endif
#if DT_ATMEL_SAM0_GCLK_4_BASE_ADDRESS
	BUILD_ASSERT(DT_ATMEL_SAM0_GCLK_4_CLOCK_FREQUENCY <=
		     DT_ATMEL_SAM0_GCLK_4_CLOCK_0_CLOCK_FREQUENCY);
	SOC_SAM0_SETUP_GCLK(4);
#endif
#if DT_ATMEL_SAM0_GCLK_5_BASE_ADDRESS
	BUILD_ASSERT(DT_ATMEL_SAM0_GCLK_5_CLOCK_FREQUENCY <=
		     DT_ATMEL_SAM0_GCLK_5_CLOCK_0_CLOCK_FREQUENCY);
	SOC_SAM0_SETUP_GCLK(5);
#endif
#if DT_ATMEL_SAM0_GCLK_6_BASE_ADDRESS
	BUILD_ASSERT(DT_ATMEL_SAM0_GCLK_6_CLOCK_FREQUENCY <=
		     DT_ATMEL_SAM0_GCLK_6_CLOCK_0_CLOCK_FREQUENCY);
	SOC_SAM0_SETUP_GCLK(6);
#endif
#if DT_ATMEL_SAM0_GCLK_7_BASE_ADDRESS
	BUILD_ASSERT(DT_ATMEL_SAM0_GCLK_7_CLOCK_FREQUENCY <=
		     DT_ATMEL_SAM0_GCLK_7_CLOCK_0_CLOCK_FREQUENCY);
	SOC_SAM0_SETUP_GCLK(7);
#endif
#if DT_ATMEL_SAM0_GCLK_8_BASE_ADDRESS
	BUILD_ASSERT(DT_ATMEL_SAM0_GCLK_8_CLOCK_FREQUENCY <=
		     DT_ATMEL_SAM0_GCLK_8_CLOCK_0_CLOCK_FREQUENCY);
	SOC_SAM0_SETUP_GCLK(8);
#endif

	/*
	 * The DFLL/DPLL can take a GCLK input, so do them after the
	 * routing is set up.
	 */
	soc_sam0_dfll_init();
	soc_sam0_fdpll_init();
	soc_sam0_wait_programmable_osc_ready();

	/*
	 * GCLK0 is the CPU clock, so it must be present.  This is also last
	 * so that everything else is set up beforehand.
	 */
	BUILD_ASSERT(DT_ATMEL_SAM0_GCLK_0_CLOCK_FREQUENCY <=
		     DT_ATMEL_SAM0_GCLK_0_CLOCK_0_CLOCK_FREQUENCY);
	SOC_SAM0_SETUP_GCLK(0);

	soc_sam0_dividers_init(CONFIG_SYS_CLOCK_HW_CYCLES_PER_SEC);
	soc_sam0_osc_ondemand();
}
