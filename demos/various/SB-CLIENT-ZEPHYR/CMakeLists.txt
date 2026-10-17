# ChibiOS - Copyright (C) 2006-2026 Giovanni Di Sirio.
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

# ChibiOS tree and Zephyr bindings module.
set(CHIBIOS ${CMAKE_CURRENT_LIST_DIR}/../../..)
list(APPEND ZEPHYR_EXTRA_MODULES ${CHIBIOS}/os/various/zephyr_bindings)

# Sandbox board, the memory of the sandbox slot is selected by SB_SLOT, it
# must match the host configuration.
set(BOARD chibios_sb)
set(SB_SLOT 128k-08060000-16k-2001C000 CACHE STRING "Sandbox slot")
list(APPEND EXTRA_DTC_OVERLAY_FILE
     ${CMAKE_CURRENT_LIST_DIR}/slots/${SB_SLOT}.overlay)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(sb_client_zephyr)

target_sources(app PRIVATE src/main.c)
