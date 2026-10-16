/*
    ChibiOS - Copyright (C) 2006-2026 Giovanni Di Sirio.

    This file is part of ChibiOS.

    ChibiOS is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation version 3 of the License.

    ChibiOS is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/**
 * @file    freertos_bindings/portable/GCC/ARMvx-M-SB/porthooks.c
 * @brief   FreeRTOS ARMvx-M-SB default application hooks.
 * @details Default implementations of the FreeRTOS application hooks
 *          suitable for a sandbox guest. The hooks are only invoked if
 *          enabled in @p FreeRTOSConfig.h, all of them are weak and can
 *          be redefined by the application.
 *
 * @addtogroup FREERTOS_ARMVXM_SB
 * @{
 */

#include "FreeRTOS.h"
#include "task.h"

/*===========================================================================*/
/* Module local definitions.                                                 */
/*===========================================================================*/

/*===========================================================================*/
/* Module exported variables.                                                */
/*===========================================================================*/

/*===========================================================================*/
/* Module local types.                                                       */
/*===========================================================================*/

/*===========================================================================*/
/* Module local variables.                                                   */
/*===========================================================================*/

/*===========================================================================*/
/* Module local functions.                                                   */
/*===========================================================================*/

/*===========================================================================*/
/* Module exported functions.                                                */
/*===========================================================================*/

/**
 * @brief   Idle hook.
 * @details Releases the CPU to the sandbox host until a VRQ is pending,
 *          it requires @p configUSE_IDLE_HOOK set to 1.
 */
__attribute__((weak)) void vApplicationIdleHook(void) {

  vPortWaitForVRQ();
}

/**
 * @brief   Heap allocation failure hook.
 * @details Terminates the sandbox with exit code
 *          @p portSB_EXIT_MALLOC_FAILED, it requires
 *          @p configUSE_MALLOC_FAILED_HOOK set to 1.
 */
__attribute__((weak)) void vApplicationMallocFailedHook(void) {

  portDISABLE_INTERRUPTS();
  sbExit((msg_t)portSB_EXIT_MALLOC_FAILED);
}

/**
 * @brief   Stack overflow hook.
 * @details Terminates the sandbox with exit code
 *          @p portSB_EXIT_STACK_OVERFLOW, it requires
 *          @p configCHECK_FOR_STACK_OVERFLOW greater than 0.
 *
 * @param[in] xTask         handle of the overflowing task
 * @param[in] pcTaskName    name of the overflowing task
 */
__attribute__((weak)) void vApplicationStackOverflowHook(TaskHandle_t xTask,
                                                         char *pcTaskName) {

  (void)xTask;
  (void)pcTaskName;

  portDISABLE_INTERRUPTS();
  sbExit((msg_t)portSB_EXIT_STACK_OVERFLOW);
}

/**
 * @brief   Assertion failure handler.
 * @details Terminates the sandbox with exit code @p portSB_EXIT_ASSERT
 *          combined with the lower 16 bits of the line number. It is meant
 *          to be invoked by @p configASSERT() in @p FreeRTOSConfig.h.
 *
 * @param[in] pcFile        source file of the failed assertion
 * @param[in] ulLine        source line of the failed assertion
 */
__attribute__((weak)) void vAssertCalled(const char *pcFile,
                                         unsigned long ulLine) {

  (void)pcFile;

  portDISABLE_INTERRUPTS();
  sbExit((msg_t)(portSB_EXIT_ASSERT | (ulLine & 0xFFFFU)));
}

/** @} */
