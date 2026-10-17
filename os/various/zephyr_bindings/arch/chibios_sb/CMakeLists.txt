# ChibiOS - Copyright (C) 2006-2026 Giovanni Di Sirio.
# SPDX-License-Identifier: Apache-2.0

# The compiler target files of Zephyr do not know this architecture, the
# code generation options are set here, also for the link step so that the
# matching libgcc multilib is selected.
set(CHIBIOS_SB_TARGET_FLAGS
  -mcpu=${CONFIG_CHIBIOS_SB_GCC_CPU}
  -mthumb
  -mabi=aapcs
)
if(CONFIG_FPU)
  list(APPEND CHIBIOS_SB_TARGET_FLAGS
    -mfloat-abi=hard
    -mfpu=${CONFIG_CHIBIOS_SB_GCC_FPU}
  )
else()
  list(APPEND CHIBIOS_SB_TARGET_FLAGS
    -mfloat-abi=soft
  )
endif()
zephyr_compile_options(${CHIBIOS_SB_TARGET_FLAGS})
zephyr_ld_options(${CHIBIOS_SB_TARGET_FLAGS})

# Needed for objcopy.
set_property(GLOBAL PROPERTY PROPERTY_OUTPUT_FORMAT elf32-littlearm)

# Public architecture headers and the sandbox ABI definitions.
zephyr_include_directories(${CMAKE_CURRENT_LIST_DIR}/../../include)

add_subdirectory(core)
