# SPDX-License-Identifier: Apache-2.0

zephyr_sources_ifdef(CONFIG_SOC_SERIES_SAMD20 clock_samd2x.c)
zephyr_sources_ifdef(CONFIG_SOC_SERIES_SAMD21 clock_samd2x.c)