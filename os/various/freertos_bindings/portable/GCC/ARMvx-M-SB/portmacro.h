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
 * @file    freertos_bindings/portable/GCC/ARMvx-M-SB/portmacro.h
 * @brief   FreeRTOS ARMvx-M-SB port macros and structures.
 * @details FreeRTOS port running as a ChibiOS sandbox guest. The guest
 *          runs unprivileged, interrupts are replaced by sandbox Virtual
 *          IRQs (VRQs) and context switches are performed in user mode.
 *
 * @addtogroup FREERTOS_ARMVXM_SB
 * @{
 */

#ifndef PORTMACRO_H
#define PORTMACRO_H

#include <stdbool.h>
#include <stdint.h>

#include "sbuser.h"

/*===========================================================================*/
/* Module constants.                                                         */
/*===========================================================================*/

/**
 * @name    Type definitions
 * @{
 */
#define portCHAR                        char
#define portFLOAT                       float
#define portDOUBLE                      double
#define portLONG                        long
#define portSHORT                       short
#define portSTACK_TYPE                  uint32_t
#define portBASE_TYPE                   long

typedef portSTACK_TYPE StackType_t;
typedef long BaseType_t;
typedef unsigned long UBaseType_t;

#if configTICK_TYPE_WIDTH_IN_BITS == TICK_TYPE_WIDTH_16_BITS
typedef uint16_t TickType_t;
#define portMAX_DELAY                   ((TickType_t)0xFFFFU)

#elif configTICK_TYPE_WIDTH_IN_BITS == TICK_TYPE_WIDTH_32_BITS
typedef uint32_t TickType_t;
#define portMAX_DELAY                   ((TickType_t)0xFFFFFFFFUL)
#define portTICK_TYPE_IS_ATOMIC         1

#else
#error "configTICK_TYPE_WIDTH_IN_BITS set to unsupported tick type width"
#endif
/** @} */

/**
 * @name    Architecture specifics
 * @{
 */
#define portARCH_NAME                   "ARMvx-M-SB"
#define portSTACK_GROWTH                (-1)
#define portTICK_PERIOD_MS              ((TickType_t)1000 / configTICK_RATE_HZ)
#define portBYTE_ALIGNMENT              8
#define portDONT_DISCARD                __attribute__((used))
#define portNOP()
#define portINLINE                      __inline
#ifndef portFORCE_INLINE
#define portFORCE_INLINE                inline __attribute__((always_inline))
#endif
#define portMEMORY_BARRIER()            __asm volatile ("" ::: "memory")
/** @} */

/**
 * @brief   VRQs globally disabled bit in the VRQ ISR register.
 */
#define portVRQ_ISR_DISABLED            1U

/**
 * @brief   Number of VRQs.
 */
#define portVRQ_NUMBER                  32U

/**
 * @brief   VRQ used by the port for the system tick.
 */
#define portVRQ_TICK                    SB_VRQ_ALARM

/**
 * @name    Sandbox exit codes of the default hooks
 * @{
 */
#define portSB_EXIT_ASSERT              0xA5000000U
#define portSB_EXIT_STACK_OVERFLOW      0xA6000000U
#define portSB_EXIT_MALLOC_FAILED       0xA7000000U
/** @} */

/*===========================================================================*/
/* Module pre-compile time settings.                                         */
/*===========================================================================*/

#ifndef configUSE_PORT_OPTIMISED_TASK_SELECTION
#define configUSE_PORT_OPTIMISED_TASK_SELECTION 1
#endif

/*===========================================================================*/
/* Derived constants and error checks.                                       */
/*===========================================================================*/

#if defined(configUSE_TICKLESS_IDLE) && (configUSE_TICKLESS_IDLE != 0)
#error "tickless idle not implemented in this port"
#endif

#if (configUSE_PORT_OPTIMISED_TASK_SELECTION == 1) &&                       \
    (configMAX_PRIORITIES > 32)
#error "optimised task selection requires configMAX_PRIORITIES <= 32"
#endif

/*===========================================================================*/
/* Module macros.                                                            */
/*===========================================================================*/

/**
 * @name    Scheduler utilities
 * @{
 */
#define portYIELD()                     vPortYield()

#define portEND_SWITCHING_ISR(xSwitchRequired)                              \
  do {                                                                      \
    if ((xSwitchRequired) != pdFALSE) {                                     \
      traceISR_EXIT_TO_SCHEDULER();                                         \
      vPortYieldFromISR();                                                  \
    }                                                                       \
    else {                                                                  \
      traceISR_EXIT();                                                      \
    }                                                                       \
  } while (0)

#define portYIELD_FROM_ISR(x)           portEND_SWITCHING_ISR(x)
/** @} */

/**
 * @name    Critical section management
 * @{
 */
#define portSET_INTERRUPT_MASK_FROM_ISR()       uxPortSetInterruptMask()
#define portCLEAR_INTERRUPT_MASK_FROM_ISR(x)    vPortClearInterruptMask(x)
#define portDISABLE_INTERRUPTS()                __sb_vrq_disable()
#define portENABLE_INTERRUPTS()                 __sb_vrq_enable()
#define portENTER_CRITICAL()                    vPortEnterCritical()
#define portEXIT_CRITICAL()                     vPortExitCritical()
/** @} */

/**
 * @name    Task function macros
 * @{
 */
#define portTASK_FUNCTION_PROTO(vFunction, pvParameters)                    \
  void vFunction(void *pvParameters)
#define portTASK_FUNCTION(vFunction, pvParameters)                          \
  void vFunction(void *pvParameters)
/** @} */

/**
 * @name    VRQ utilities
 * @{
 */
/**
 * @brief   VRQ handler definition.
 * @details Handlers run with VRQs disabled on the stack of the interrupted
 *          task, they can use the FreeRTOS "FromISR" API and
 *          @p portYIELD_FROM_ISR().
 * @note    @p n must be a decimal integer literal without suffixes, or a
 *          macro expanding to one, because it is pasted into the vector
 *          name. A handler with a malformed name is never invoked.
 * @note    @p portVRQ_TICK is used by the port.
 *
 * @param[in] n         VRQ number
 */
#define portVRQ_HANDLER(n)              portVRQ_HANDLER_PASTE(n)
#define portVRQ_HANDLER_PASTE(n)        void __sb_vector##n(void)
/** @} */

/**
 * @name    Optimised task selection
 * @{
 */
#if (configUSE_PORT_OPTIMISED_TASK_SELECTION == 1) || defined(__DOXYGEN__)
#define portRECORD_READY_PRIORITY(uxPriority, uxReadyPriorities)            \
  (uxReadyPriorities) |= (1UL << (uxPriority))

#define portRESET_READY_PRIORITY(uxPriority, uxReadyPriorities)             \
  (uxReadyPriorities) &= ~(1UL << (uxPriority))

#define portGET_HIGHEST_PRIORITY(uxTopPriority, uxReadyPriorities)          \
  uxTopPriority = (31UL - (uint32_t)__builtin_clz(uxReadyPriorities))
#endif
/** @} */

/*===========================================================================*/
/* External declarations.                                                    */
/*===========================================================================*/

extern bool __sb_in_vrq;

#ifdef __cplusplus
extern "C" {
#endif
  void vPortYield(void);
  void vPortYieldFromISR(void);
  void vPortEnterCritical(void);
  void vPortExitCritical(void);
#ifdef __cplusplus
}
#endif

/*===========================================================================*/
/* Module inline functions.                                                  */
/*===========================================================================*/

/**
 * @brief   Determines the current execution context.
 *
 * @return              The execution context.
 * @retval pdFALSE      not running in a VRQ handler.
 * @retval pdTRUE       running in a VRQ handler.
 */
portFORCE_INLINE static BaseType_t xPortIsInsideInterrupt(void) {

  return __sb_in_vrq ? pdTRUE : pdFALSE;
}

/**
 * @brief   Disables VRQs returning the previous VRQ status.
 * @note    VRQs are already disabled while running a VRQ handler, in that
 *          case no further action is taken.
 *
 * @return              The previous VRQ ISR register value.
 */
portFORCE_INLINE static UBaseType_t uxPortSetInterruptMask(void) {
  UBaseType_t uxSts;

  uxSts = (UBaseType_t)__sb_vrq_getisr();
  if ((uxSts & portVRQ_ISR_DISABLED) == 0U) {
    __sb_vrq_disable();
  }

  return uxSts;
}

/**
 * @brief   Restores the VRQ status returned by @p uxPortSetInterruptMask().
 *
 * @param[in] uxSts     the VRQ ISR register value to be restored
 */
portFORCE_INLINE static void vPortClearInterruptMask(UBaseType_t uxSts) {

  if ((uxSts & portVRQ_ISR_DISABLED) == 0U) {
    __sb_vrq_enable();
  }
}

/**
 * @brief   Releases the CPU to the sandbox host until a VRQ is pending.
 * @details It is meant to be called from @p vApplicationIdleHook() in order
 *          to not consume the host CPU time while the guest is idle.
 */
portFORCE_INLINE static void vPortWaitForVRQ(void) {

  __sb_vrq_wait();
}

/**
 * @brief   Enables a VRQ.
 * @note    @p portVRQ_TICK is enabled by the port when the scheduler
 *          starts.
 *
 * @param[in] uxVRQ     VRQ number
 */
portFORCE_INLINE static void vPortVRQEnable(UBaseType_t uxVRQ) {

  configASSERT(uxVRQ < portVRQ_NUMBER);

  (void)__sb_vrq_seten(1UL << uxVRQ);
}

/**
 * @brief   Disables a VRQ.
 *
 * @param[in] uxVRQ     VRQ number
 */
portFORCE_INLINE static void vPortVRQDisable(UBaseType_t uxVRQ) {

  configASSERT(uxVRQ < portVRQ_NUMBER);
  configASSERT(uxVRQ != portVRQ_TICK);

  (void)__sb_vrq_clren(1UL << uxVRQ);
}

/**
 * @brief   Triggers a VRQ on the guest itself.
 * @details The VRQ handler is invoked immediately if the VRQ is enabled and
 *          VRQs are not disabled, else the VRQ remains pending.
 *
 * @param[in] uxVRQ     VRQ number
 */
portFORCE_INLINE static void vPortVRQTrigger(UBaseType_t uxVRQ) {

  configASSERT(uxVRQ < portVRQ_NUMBER);

  (void)__sb_vrq_setwt(1UL << uxVRQ);
}

/**
 * @brief   Returns and clears the flags associated to a VRQ.
 * @details Flags are set by the host, for example by VIO drivers, in order
 *          to report the reason of a VRQ.
 *
 * @param[in] uxVRQ     VRQ number
 * @return              The VRQ flags.
 */
portFORCE_INLINE static uint32_t ulPortVRQGetFlags(UBaseType_t uxVRQ) {

  configASSERT(uxVRQ < portVRQ_NUMBER);

  return __sb_vrq_gcsts((uint32_t)uxVRQ);
}

#endif /* PORTMACRO_H */

/** @} */
