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
 * @file    arch/chibios_sb/core/cpu_idle.c
 * @brief   ChibiOS sandbox architecture idle.
 * @details The CPU is released to the host until a VRQ is pending. The
 *          host wakes the sandbox also with VRQs globally disabled, the
 *          pending VRQ is served when VRQs are enabled again.
 *
 * @addtogroup ZEPHYR_CHIBIOS_SB
 * @{
 */

#include <zephyr/kernel.h>
#include <zephyr/tracing/tracing.h>

/*===========================================================================*/
/* Module exported functions.                                                */
/*===========================================================================*/

/**
 * @brief   Waits for a VRQ, VRQs are enabled on return.
 * @note    Invoked with VRQs disabled.
 */
void arch_cpu_idle(void) {

#if defined(CONFIG_SYS_IDLE_HOOKS)
  sys_trace_idle();
#endif
  chibios_sb_vrq_wait();
#if defined(CONFIG_SYS_IDLE_HOOKS)
  sys_trace_idle_exit();
#endif
  chibios_sb_vrq_enable();
}

/**
 * @brief   Waits for a VRQ, then restores the VRQ lock state.
 * @note    Invoked with VRQs disabled.
 *
 * @param[in] key       the lock key to be restored
 */
void arch_cpu_atomic_idle(unsigned int key) {

#if defined(CONFIG_SYS_IDLE_HOOKS)
  sys_trace_idle();
#endif
  chibios_sb_vrq_wait();
#if defined(CONFIG_SYS_IDLE_HOOKS)
  sys_trace_idle_exit();
#endif
  arch_irq_unlock(key);
}

/** @} */
