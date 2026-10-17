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
 * @file    arch/chibios_sb/core/irq_offload.c
 * @brief   ChibiOS sandbox architecture irq_offload().
 * @details The routine runs in the handler of a VRQ triggered by the guest
 *          itself, @p CONFIG_CHIBIOS_SB_IRQ_OFFLOAD_VRQ.
 * @note    If VRQs are disabled when irq_offload() is invoked then the
 *          routine runs when VRQs are enabled again.
 *
 * @addtogroup ZEPHYR_CHIBIOS_SB
 * @{
 */

#include <zephyr/kernel.h>
#include <zephyr/irq.h>
#include <zephyr/irq_offload.h>

/*===========================================================================*/
/* Module local variables.                                                   */
/*===========================================================================*/

static irq_offload_routine_t offload_routine;
static const void *offload_parameter;

/*===========================================================================*/
/* Module local functions.                                                   */
/*===========================================================================*/

static void offload_isr(const void *arg) {
  irq_offload_routine_t routine = offload_routine;

  ARG_UNUSED(arg);

  if (routine != NULL) {
    offload_routine = NULL;
    routine(offload_parameter);
  }
}

/*===========================================================================*/
/* Module exported functions.                                                */
/*===========================================================================*/

/**
 * @brief   Runs a routine in VRQ context.
 *
 * @param[in] routine   the routine
 * @param[in] parameter the routine parameter
 */
void arch_irq_offload(irq_offload_routine_t routine, const void *parameter) {

  offload_parameter = parameter;
  offload_routine = routine;
  (void)chibios_sb_vrq_setwt(BIT(CONFIG_CHIBIOS_SB_IRQ_OFFLOAD_VRQ));
}

/**
 * @brief   Connects and enables the offload VRQ.
 */
void arch_irq_offload_init(void) {

  IRQ_CONNECT(CONFIG_CHIBIOS_SB_IRQ_OFFLOAD_VRQ, 0, offload_isr, NULL, 0);
  irq_enable(CONFIG_CHIBIOS_SB_IRQ_OFFLOAD_VRQ);
}

/** @} */
