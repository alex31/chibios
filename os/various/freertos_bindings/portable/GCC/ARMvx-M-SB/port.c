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
 * @file    freertos_bindings/portable/GCC/ARMvx-M-SB/port.c
 * @brief   FreeRTOS ARMvx-M-SB port code.
 * @details Context switches are performed in user mode by a function call
 *          saving the callee-saved registers on the outgoing task stack,
 *          the caller-saved registers are either dead at the call site or
 *          already saved in the VRQ frame built by the sandbox host.
 *          Switches are performed with VRQs disabled and with no critical
 *          zone active, a yield requested inside a critical zone or by a
 *          VRQ handler is deferred to the critical zone exit or to the VRQ
 *          epilogue, the same way a pended PendSV would be served.
 *          Ticks are accounted from the host system time, ticks elapsed
 *          while the sandbox was not running are recovered.
 *          The scheduler can be stopped using @p vTaskEndScheduler(),
 *          @p vTaskStartScheduler() then returns to its caller.
 *
 * @addtogroup FREERTOS_ARMVXM_SB
 * @{
 */

#include "FreeRTOS.h"
#include "task.h"

/*===========================================================================*/
/* Module local definitions.                                                 */
/*===========================================================================*/

/**
 * @brief   Critical nesting value before the scheduler is started.
 * @details Critical zones entered before starting the scheduler leave
 *          VRQs disabled, VRQs are enabled when the first task starts.
 */
#define portINITIAL_CRITICAL_NESTING    0xAAAAAAAAUL

/**
 * @brief   Address a task returns to when exiting its function.
 */
#if defined(configTASK_RETURN_ADDRESS)
#define portTASK_RETURN_ADDRESS         configTASK_RETURN_ADDRESS
#else
#define portTASK_RETURN_ADDRESS         prvTaskExitError
#endif

/*===========================================================================*/
/* Module exported variables.                                                */
/*===========================================================================*/

/*===========================================================================*/
/* Module local types.                                                       */
/*===========================================================================*/

/**
 * @brief   Task saved context.
 * @details This structure represents the stack frame saved by
 *          @p vPortSwitchContext().
 * @note    R3 is not part of the context, it is only saved in order to
 *          keep the stack aligned to 8 bytes.
 */
struct port_intctx {
#if defined(__ARM_FP)
  uint32_t      s16_s31[16];
#endif
  uint32_t      r3;
  uint32_t      r4;
  uint32_t      r5;
  uint32_t      r6;
  uint32_t      r7;
  uint32_t      r8;
  uint32_t      r9;
  uint32_t      r10;
  uint32_t      r11;
  uint32_t      lr;
};

/*===========================================================================*/
/* Module local variables.                                                   */
/*===========================================================================*/

/**
 * @brief   Critical zones nesting counter.
 * @note    It is not part of the task context because switches are only
 *          performed when it is zero.
 */
static UBaseType_t uxCriticalNesting = portINITIAL_CRITICAL_NESTING;

/**
 * @brief   Deferred context switch request.
 */
static volatile BaseType_t xSwitchPending = pdFALSE;

/**
 * @brief   Tick period in host system time units.
 */
static sysinterval_t xTickInterval;

/**
 * @brief   Host system time of the last accounted tick.
 */
static systime_t xLastTickTime;

/**
 * @brief   Context saved when starting the scheduler.
 * @details It is resumed when the scheduler is stopped.
 */
static void *pvSchedulerCallerContext;

/*===========================================================================*/
/* Module local functions.                                                   */
/*===========================================================================*/

/* Low level code in portasm.S.*/
void vPortSwitchContext(void);
void vPortStartFirstTask(void **ppvContext);
void vPortRestoreContext(void *pvContext) __attribute__((noreturn));
void vPortTaskStart(void);

/* Invoked by the VRQ entry code in vectors.S.*/
void vPortVRQEpilogue(void);

#if !defined(configTASK_RETURN_ADDRESS)
/**
 * @brief   Catches tasks returning from their function.
 * @note    Tasks must delete themselves using @p vTaskDelete(NULL) instead
 *          of returning.
 */
static void prvTaskExitError(void) {

  /* Intentionally failing assertion, same as the other ports.*/
  configASSERT(uxCriticalNesting == ~0UL);

  portDISABLE_INTERRUPTS();
  for (;;) {
  }
}
#endif

/*===========================================================================*/
/* Module VRQ handlers.                                                      */
/*===========================================================================*/

/**
 * @brief   Tick VRQ handler.
 * @details All the tick periods elapsed since the last accounted tick are
 *          accounted. The host merges the alarm VRQs triggered while the
 *          sandbox is not running, those ticks are recovered here.
 * @note    The periodic host alarm is delivered on @p portVRQ_TICK.
 */
portVRQ_HANDLER(portVRQ_TICK) {
  systime_t xNow;
  BaseType_t xSwitchRequired = pdFALSE;

  traceISR_ENTER();

  xNow = sbGetSystemTime();

  /* A negative difference is only possible with a host system time
     narrower than 32 bits, the elapsed ticks are lost in that case.*/
  if ((int32_t)(xNow - xLastTickTime) < 0) {
    xLastTickTime = xNow;
  }

  while ((sysinterval_t)(xNow - xLastTickTime) >= xTickInterval) {
    xLastTickTime += xTickInterval;
    if (xTaskIncrementTick() != pdFALSE) {
      xSwitchRequired = pdTRUE;
    }
  }

  if (xSwitchRequired != pdFALSE) {
    traceISR_EXIT_TO_SCHEDULER();
    xSwitchPending = pdTRUE;
  }
  else {
    traceISR_EXIT();
  }
}

/**
 * @brief   VRQ epilogue hook.
 * @details Performs the context switch requested by VRQ handlers. It is
 *          invoked by the common VRQ entry code with VRQs disabled, the
 *          VRQ frame of a switched-out task is popped when that task is
 *          switched in again.
 */
void vPortVRQEpilogue(void) {

  if ((xSwitchPending != pdFALSE) && (uxCriticalNesting == 0U)) {
    xSwitchPending = pdFALSE;
    vPortSwitchContext();
  }
}

/*===========================================================================*/
/* Module exported functions.                                                */
/*===========================================================================*/

/**
 * @brief   Initializes the context of a new task.
 * @details The context is placed right below the top of stack so that the
 *          task starts with the stack pointer at @p pxTopOfStack, which is
 *          aligned to 8 bytes by the kernel.
 *
 * @param[in] pxTopOfStack  aligned top of the task stack
 * @param[in] pxCode        task function
 * @param[in] pvParameters  task function parameter
 * @return                  The initial task stack pointer.
 */
StackType_t *pxPortInitialiseStack(StackType_t *pxTopOfStack,
                                   TaskFunction_t pxCode,
                                   void *pvParameters) {
  struct port_intctx *ctxp;

  ctxp = (struct port_intctx *)(void *)pxTopOfStack - 1;
  ctxp->r4 = (uint32_t)pxCode;
  ctxp->r5 = (uint32_t)pvParameters;
  ctxp->r6 = (uint32_t)portTASK_RETURN_ADDRESS;
  ctxp->lr = (uint32_t)vPortTaskStart;

  return (StackType_t *)(void *)ctxp;
}

/**
 * @brief   Sets up the tick source.
 * @details The tick is generated by a periodic host alarm delivered as a
 *          VRQ, the tick period is rounded down to the host time
 *          resolution.
 * @note    It is a weak function, an application can redefine it in order
 *          to use a different tick source, the tick VRQ handler still
 *          accounts ticks from the host system time.
 */
__attribute__((weak)) void vPortSetupTimerInterrupt(void) {

  vPortVRQEnable(portVRQ_TICK);
  (void)sbSetAlarm(xTickInterval, true);
}

/**
 * @brief   Starts the scheduler.
 * @note    VRQs have been disabled by the kernel at this point, they are
 *          enabled when the first task starts.
 *
 * @return              It only returns after @p vTaskEndScheduler().
 * @retval pdFALSE      the scheduler has been stopped.
 */
BaseType_t xPortStartScheduler(void) {

  xTickInterval = sbGetFrequency() / (sysinterval_t)configTICK_RATE_HZ;

  configASSERT(xTickInterval > 0U);

  /* Tick accounting base, taken before arming the alarm.*/
  xLastTickTime = sbGetSystemTime();
  vPortSetupTimerInterrupt();

  uxCriticalNesting = 0U;
  xSwitchPending = pdFALSE;

  /* The current context is saved, it is resumed by vPortEndScheduler().*/
  vPortStartFirstTask(&pvSchedulerCallerContext);

  return pdFALSE;
}

/**
 * @brief   Stops the scheduler.
 * @details Invoked by @p vTaskEndScheduler() with VRQs disabled. The tick
 *          alarm is stopped and the context saved by
 *          @p xPortStartScheduler() is resumed, @p vTaskStartScheduler()
 *          returns with VRQs disabled, as before starting the scheduler.
 * @note    The stack of the calling task is abandoned, the application is
 *          responsible for deleting that task and the other remaining
 *          ones, if required.
 * @note    The sandbox terminates when @p main() returns, the value
 *          returned by @p main() is the exit code.
 * @note    A tick source set up by a redefined
 *          @p vPortSetupTimerInterrupt() is not stopped.
 */
void vPortEndScheduler(void) {

  configASSERT(xPortIsInsideInterrupt() == pdFALSE);

  sbResetAlarm();

  /* Same state as before starting the scheduler, critical zones leave
     VRQs disabled.*/
  uxCriticalNesting = portINITIAL_CRITICAL_NESTING;
  xSwitchPending    = pdFALSE;

  vPortRestoreContext(pvSchedulerCallerContext);
}

/**
 * @brief   Yields the CPU.
 * @details The switch is performed immediately unless a critical zone is
 *          active, in that case it is deferred to the critical zone exit.
 */
void vPortYield(void) {

  configASSERT(xPortIsInsideInterrupt() == pdFALSE);

  if (uxCriticalNesting == 0U) {
    portDISABLE_INTERRUPTS();
    xSwitchPending = pdFALSE;
    vPortSwitchContext();
    portENABLE_INTERRUPTS();
  }
  else {
    xSwitchPending = pdTRUE;
  }
}

/**
 * @brief   Yields the CPU from a VRQ handler.
 * @details In a VRQ handler the switch is deferred to the VRQ epilogue,
 *          in task context it is equivalent to @p vPortYield().
 */
void vPortYieldFromISR(void) {

  if (xPortIsInsideInterrupt() != pdFALSE) {
    xSwitchPending = pdTRUE;
  }
  else {
    vPortYield();
  }
}

/**
 * @brief   Enters a critical zone.
 */
void vPortEnterCritical(void) {

  configASSERT(xPortIsInsideInterrupt() == pdFALSE);

  portDISABLE_INTERRUPTS();
  uxCriticalNesting++;
}

/**
 * @brief   Leaves a critical zone.
 * @details A switch requested inside the critical zone is performed when
 *          leaving the outermost one, before re-enabling VRQs.
 */
void vPortExitCritical(void) {

  configASSERT(uxCriticalNesting > 0U);

  uxCriticalNesting--;
  if (uxCriticalNesting == 0U) {
    if (xSwitchPending != pdFALSE) {
      xSwitchPending = pdFALSE;
      vPortSwitchContext();
    }
    portENABLE_INTERRUPTS();
  }
}

/** @} */
