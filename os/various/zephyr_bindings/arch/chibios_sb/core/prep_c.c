/*
    ChibiOS - Copyright (C) 2006-2026 Giovanni Di Sirio.

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
*/

/**
 * @file    arch/chibios_sb/core/prep_c.c
 * @brief   ChibiOS sandbox architecture C environment initialization.
 *
 * @addtogroup ZEPHYR_CHIBIOS_SB
 * @{
 */

#include <zephyr/kernel.h>
#include <zephyr/arch/common/init.h>
#include <zephyr/arch/common/xip.h>
#include <zephyr/linker/linker-defs.h>
#include <kernel_internal.h>

/* The entry code places the initial stack pointer at the end of the
   interrupt stack using the configured size.*/
BUILD_ASSERT(sizeof (z_interrupt_stacks[0]) == CONFIG_ISR_STACK_SIZE,
             "interrupt stack size mismatch");
BUILD_ASSERT(K_KERNEL_STACK_RESERVED == 0,
             "reserved area in the interrupt stack");

/*===========================================================================*/
/* Module exported functions.                                                */
/*===========================================================================*/

/**
 * @brief   Initializes the C environment and starts the kernel.
 * @note    Invoked by the entry code with VRQs disabled.
 *
 * @param[in] sysblk    address of the host system block
 */
FUNC_NORETURN void z_chibios_sb_prep_c(uintptr_t sysblk) {

  /* The host keeps the VRQ state in the system block at the top of the
     data region, the slot overlay must exclude it from the kernel RAM.*/
  if (sysblk < (uintptr_t)__kernel_ram_end) {
    chibios_sb_exit((uint32_t)CHIBIOS_SB_EXIT_HALT | K_ERR_KERNEL_PANIC);
  }

  arch_bss_zero();
  arch_data_copy();
  z_cstart();
  CODE_UNREACHABLE;
}

/** @} */
