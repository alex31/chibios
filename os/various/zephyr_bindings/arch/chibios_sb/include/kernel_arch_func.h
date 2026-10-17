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
 * @file    arch/chibios_sb/include/kernel_arch_func.h
 * @brief   ChibiOS sandbox architecture private kernel functions.
 *
 * @addtogroup ZEPHYR_CHIBIOS_SB
 * @{
 */

#ifndef ZEPHYR_ARCH_CHIBIOS_SB_INCLUDE_KERNEL_ARCH_FUNC_H_
#define ZEPHYR_ARCH_CHIBIOS_SB_INCLUDE_KERNEL_ARCH_FUNC_H_

#if !defined(_ASMLANGUAGE)

#include <kernel_arch_data.h>

#ifdef __cplusplus
extern "C" {
#endif
  void z_chibios_sb_switch(void *switch_to, void **switched_from);
#ifdef __cplusplus
}
#endif

/**
 * @brief   Architecture-specific kernel initialization, nothing to do.
 */
static ALWAYS_INLINE void arch_kernel_init(void) {
}

/**
 * @brief   Context switch.
 * @details The callee-saved registers are saved on the current stack, the
 *          stack pointer is the switch handle stored in @p switched_from,
 *          then the context saved at @p switch_to is restored. It is
 *          invoked with VRQs disabled, from threads or from the VRQ
 *          epilogue.
 *
 * @param[in] switch_to         switch handle of the incoming thread
 * @param[out] switched_from    where to store the outgoing switch handle
 */
static ALWAYS_INLINE void arch_switch(void *switch_to,
                                      void **switched_from) {

  z_chibios_sb_switch(switch_to, switched_from);
}

/**
 * @brief   Checks if running in a VRQ handler.
 *
 * @return              @p true if running in a VRQ handler.
 */
static ALWAYS_INLINE bool arch_is_in_isr(void) {

  return arch_curr_cpu()->nested != 0U;
}

#endif /* !defined(_ASMLANGUAGE) */

#endif /* ZEPHYR_ARCH_CHIBIOS_SB_INCLUDE_KERNEL_ARCH_FUNC_H_ */

/** @} */
