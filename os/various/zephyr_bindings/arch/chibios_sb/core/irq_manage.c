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
 * @file    arch/chibios_sb/core/irq_manage.c
 * @brief   ChibiOS sandbox architecture VRQ management.
 * @details IRQ numbers are VRQ numbers. VRQ handlers are dispatched
 *          through the software ISR table generated at build time, they
 *          run on the stack of the interrupted thread with VRQs disabled.
 *
 * @addtogroup ZEPHYR_CHIBIOS_SB
 * @{
 */

#include <zephyr/kernel.h>
#include <zephyr/irq.h>
#include <zephyr/sw_isr_table.h>
#include <zephyr/tracing/tracing.h>
#include <kernel_arch_func.h>
#include <ksched.h>

/*===========================================================================*/
/* Module exported functions.                                                */
/*===========================================================================*/

/**
 * @brief   Enables a VRQ.
 *
 * @param[in] irq       VRQ number
 */
void arch_irq_enable(unsigned int irq) {

  __ASSERT(irq < CHIBIOS_SB_VRQ_NUMBER, "invalid VRQ %u", irq);

  (void)chibios_sb_vrq_seten(BIT(irq));
}

/**
 * @brief   Disables a VRQ.
 *
 * @param[in] irq       VRQ number
 */
void arch_irq_disable(unsigned int irq) {

  __ASSERT(irq < CHIBIOS_SB_VRQ_NUMBER, "invalid VRQ %u", irq);

  (void)chibios_sb_vrq_clren(BIT(irq));
}

/**
 * @brief   Checks if a VRQ is enabled.
 *
 * @param[in] irq       VRQ number
 * @return              Non-zero if the VRQ is enabled.
 */
int arch_irq_is_enabled(unsigned int irq) {

  __ASSERT(irq < CHIBIOS_SB_VRQ_NUMBER, "invalid VRQ %u", irq);

  return (chibios_sb_vrq_seten(0U) & BIT(irq)) != 0U;
}

/**
 * @brief   Handler of the VRQs with no connected handler.
 * @note    The host only delivers enabled VRQs.
 *
 * @param[in] unused    unused parameter
 */
void z_irq_spurious(const void *unused) {

  ARG_UNUSED(unused);

  z_fatal_error(K_ERR_SPURIOUS_IRQ, NULL);
}

/**
 * @brief   VRQ handler, invoked by the VRQ entry code.
 * @details The handler of the VRQ is invoked, then a preemption is
 *          performed if a different thread became the one to run. The
 *          switch is performed on the stack of the interrupted thread,
 *          its VRQ frame is popped when it is switched in again.
 *
 * @param[in] nvrq      VRQ number
 */
void z_chibios_sb_vrq_handler(uint32_t nvrq) {
  const struct _isr_table_entry *ite;
  struct k_thread *old;
  void *next;

  ite = &_sw_isr_table[nvrq - CONFIG_GEN_IRQ_START_VECTOR];

#if defined(CONFIG_TRACING_ISR)
  sys_trace_isr_enter();
#endif

  arch_curr_cpu()->nested++;
  ite->isr(ite->arg);
  arch_curr_cpu()->nested--;

#if defined(CONFIG_TRACING_ISR)
  sys_trace_isr_exit();
#endif

  /* Preemption, VRQs are still disabled here.*/
  old = _current;
  next = z_sched_next_handle(old);
  if (next != NULL) {
    z_chibios_sb_switch(next, &old->switch_handle);
  }
}

/** @} */
