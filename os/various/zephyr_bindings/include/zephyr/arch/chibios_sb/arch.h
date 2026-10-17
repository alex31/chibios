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
 * @file    zephyr/arch/chibios_sb/arch.h
 * @brief   ChibiOS sandbox architecture public interface.
 * @details Zephyr runs unprivileged inside a ChibiOS sandbox. Interrupts
 *          are replaced by the sandbox Virtual IRQs (VRQs): the IRQ lock
 *          is the VRQs global disable, IRQ numbers are VRQ numbers.
 *
 * @addtogroup ZEPHYR_CHIBIOS_SB
 * @{
 */

#ifndef ZEPHYR_INCLUDE_ARCH_CHIBIOS_SB_ARCH_H_
#define ZEPHYR_INCLUDE_ARCH_CHIBIOS_SB_ARCH_H_

#include <zephyr/arch/chibios_sb/sb.h>
#include <zephyr/arch/chibios_sb/thread.h>
#include <zephyr/arch/exception.h>
#include <zephyr/arch/common/sys_bitops.h>
#include <zephyr/arch/common/sys_io.h>
#include <zephyr/arch/common/ffs.h>

/*===========================================================================*/
/* Module constants.                                                         */
/*===========================================================================*/

/**
 * @brief   Stack pointer alignment, as required by the AAPCS.
 */
#define ARCH_STACK_PTR_ALIGN            8

#if !defined(_ASMLANGUAGE)

#include <zephyr/sw_isr_table.h>
#include <zephyr/kernel_structs.h>
#include <zephyr/sys/util.h>

/*===========================================================================*/
/* Module macros.                                                            */
/*===========================================================================*/

/**
 * @brief   Static VRQ handler connection.
 * @details The handler is placed in the software ISR table generated at
 *          build time, VRQs have no priority and no flags.
 */
#define ARCH_IRQ_CONNECT(irq_p, priority_p, isr_p, isr_param_p, flags_p)   \
  {                                                                         \
    Z_ISR_DECLARE(irq_p, 0, isr_p, isr_param_p);                            \
    ARG_UNUSED(priority_p);                                                 \
    ARG_UNUSED(flags_p);                                                    \
  }

/*===========================================================================*/
/* External declarations.                                                    */
/*===========================================================================*/

#ifdef __cplusplus
extern "C" {
#endif
  void z_irq_spurious(const void *unused);
  uint32_t sys_clock_cycle_get_32(void);
  uint64_t sys_clock_cycle_get_64(void);
#ifdef __cplusplus
}
#endif

/*===========================================================================*/
/* Module inline functions.                                                  */
/*===========================================================================*/

/**
 * @brief   Locks the VRQs.
 * @details The key is the previous VRQ ISR register value, nested locks
 *          return a key with the disabled bit set.
 *
 * @return              The lock key.
 */
static ALWAYS_INLINE unsigned int arch_irq_lock(void) {
  unsigned int key;

  key = chibios_sb_vrq_getisr();
  if ((key & CHIBIOS_SB_VRQ_ISR_DISABLED) == 0U) {
    chibios_sb_vrq_disable();
  }

  return key;
}

/**
 * @brief   Unlocks the VRQs.
 * @details VRQs are enabled only if the key comes from the outermost
 *          lock, pending VRQs are served at that point.
 *
 * @param[in] key       the lock key
 */
static ALWAYS_INLINE void arch_irq_unlock(unsigned int key) {

  if ((key & CHIBIOS_SB_VRQ_ISR_DISABLED) == 0U) {
    chibios_sb_vrq_enable();
  }
}

/**
 * @brief   Checks if a lock key unlocks the VRQs.
 *
 * @param[in] key       the lock key
 * @return              @p true if the key unlocks the VRQs.
 */
static ALWAYS_INLINE bool arch_irq_unlocked(unsigned int key) {

  return (key & CHIBIOS_SB_VRQ_ISR_DISABLED) == 0U;
}

/**
 * @brief   Checks if VRQs are enabled.
 *
 * @return              @p true if VRQs are enabled.
 */
static ALWAYS_INLINE bool arch_cpu_irqs_are_enabled(void) {

  return (chibios_sb_vrq_getisr() & CHIBIOS_SB_VRQ_ISR_DISABLED) == 0U;
}

/**
 * @brief   Returns the current CPU, single core.
 *
 * @return              The current CPU.
 */
static ALWAYS_INLINE _cpu_t *arch_curr_cpu(void) {

  return &_kernel.cpus[0];
}

/**
 * @brief   Hardware cycle counter, it is the host system time.
 *
 * @return              The 32 bits cycle counter.
 */
static inline uint32_t arch_k_cycle_get_32(void) {

  return sys_clock_cycle_get_32();
}

/**
 * @brief   Hardware cycle counter, it is the host system time.
 *
 * @return              The 64 bits cycle counter.
 */
static inline uint64_t arch_k_cycle_get_64(void) {

  return sys_clock_cycle_get_64();
}

/**
 * @brief   No operation.
 */
static ALWAYS_INLINE void arch_nop(void) {

  __asm__ volatile ("nop");
}

#endif /* !defined(_ASMLANGUAGE) */

#endif /* ZEPHYR_INCLUDE_ARCH_CHIBIOS_SB_ARCH_H_ */

/** @} */
