# ChibiOS - Copyright (C) 2006-2026 Giovanni Di Sirio.
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

# ChibiOS tree.
set(CHIBIOS ${CMAKE_CURRENT_LIST_DIR}/../../..)

# Zephyr tree, the one in ext/zephyr is used if ZEPHYR_BASE is not set, see
# ext/zephyr_readme.md.
if(NOT DEFINED ENV{ZEPHYR_BASE})
  set(ENV{ZEPHYR_BASE} ${CHIBIOS}/ext/zephyr)
endif()

# Zephyr bindings module. ZEPHYR_MODULES is used because, unlike
# ZEPHYR_EXTRA_MODULES, it also works outside of a west workspace, the
# modules of a west workspace are not needed.
list(APPEND ZEPHYR_MODULES ${CHIBIOS}/os/various/zephyr_bindings)

# The ARM GCC found in PATH is used if no Zephyr toolchain is selected, for
# example the ChibiStudio one.
if(NOT DEFINED ZEPHYR_TOOLCHAIN_VARIANT AND
   NOT DEFINED ENV{ZEPHYR_TOOLCHAIN_VARIANT})
  find_program(CHIBIOS_ARM_GCC arm-none-eabi-gcc)
  if(CHIBIOS_ARM_GCC)
    get_filename_component(CHIBIOS_ARM_GCC_BIN ${CHIBIOS_ARM_GCC} DIRECTORY)
    get_filename_component(GNUARMEMB_TOOLCHAIN_PATH ${CHIBIOS_ARM_GCC_BIN}
                           DIRECTORY)
    set(ZEPHYR_TOOLCHAIN_VARIANT gnuarmemb)
  endif()
endif()

# Sandbox board, the memory of the sandbox slot is selected by SB_SLOT, it
# must match the host configuration.
set(BOARD chibios_sb)
set(SB_SLOT 128k-08060000-16k-2001C000 CACHE STRING "Sandbox slot")
list(APPEND EXTRA_DTC_OVERLAY_FILE
     ${CMAKE_CURRENT_LIST_DIR}/slots/${SB_SLOT}.overlay)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(sb_client_zephyr)

target_sources(app PRIVATE src/main.c)
