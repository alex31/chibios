# ChibiOS - Copyright (C) 2006-2026 Giovanni Di Sirio.
# SPDX-License-Identifier: Apache-2.0

zephyr_library()

zephyr_library_sources(
  sb_entry.S
  cpu_idle.c
  fatal.c
  irq_manage.c
  prep_c.c
  printk.c
  thread.c
)

zephyr_library_sources_ifdef(CONFIG_IRQ_OFFLOAD irq_offload.c)
