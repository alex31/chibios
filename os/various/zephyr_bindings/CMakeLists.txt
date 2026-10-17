# ChibiOS - Copyright (C) 2006-2026 Giovanni Di Sirio.
# SPDX-License-Identifier: Apache-2.0

# Module parts not belonging to the architecture, SoC or board.

if(CONFIG_CHIBIOS_SB_TIMER)
  zephyr_library_named(chibios_sb_timer)
  zephyr_library_sources(drivers/timer/chibios_sb_timer.c)
  # The generic timer core is an implementation header of Zephyr.
  zephyr_library_include_directories(${ZEPHYR_BASE}/drivers/timer)
endif()
