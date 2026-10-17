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
 * @file    threadx_bindings/portable/GCC/ARMvx-M-SB/tx_port.c
 * @brief   ThreadX ARMvx-M-SB port code.
 * @details The scheduler runs on the stack of @p main(), threads return to
 *          it when they suspend or are preempted, it then switches to the
 *          thread in @p _tx_thread_execute_ptr. Switches are performed in
 *          user mode by a function call saving the callee-saved registers
 *          on the outgoing stack, the caller-saved registers are either
 *          dead at the call site or already saved in the VRQ frame built by
 *          the sandbox host, in FPU builds the saved context includes
 *          S16-S31 and FPSCR. A preemption requested by a VRQ handler is
 *          performed in the VRQ epilogue, the VRQ frame of a preempted
 *          thread is popped when that thread is switched in again.
 *          Ticks are accounted from the host system time, ticks elapsed
 *          while the sandbox was not running are recovered.
 *
 * @addtogroup THREADX_ARMVXM_SB
 * @{
 */

#include "tx_api.h"
#include "tx_initialize.h"
#include "tx_thread.h"
#include "tx_timer.h"

/*===========================================================================*/
/* Module local definitions.                                                 */
/*===========================================================================*/

/**
 * @brief   Initial FPSCR value of a thread, IEEE 754 default modes.
 */
#define TX_SB_INITIAL_FPSCR             0U

/*===========================================================================*/
/* Module local types.                                                       */
/*===========================================================================*/

/**
 * @brief   Context saved by @p _tx_sb_switch().
 * @note    In FPU builds the @p fpscr field holds FPSCR, else it holds R3
 *          which is only saved in order to keep the stack aligned to 8
 *          bytes.
 */
struct port_switch_frame {
#if defined(__ARM_FP)
  uint32_t      s16_s31[16];
#endif
  uint32_t      fpscr;
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
 * @brief   Saved context of the scheduler.
 */
static VOID *_tx_sb_scheduler_sp;

/**
 * @brief   Tick period in host system time units.
 */
static sysinterval_t _tx_sb_tick_interval;

/**
 * @brief   Host system time of the last accounted tick.
 */
static systime_t _tx_sb_last_tick_time;

/*===========================================================================*/
/* Module local functions.                                                   */
/*===========================================================================*/

/* Low level code in tx_port_asm.S.*/
VOID _tx_sb_switch(VOID *switch_to, VOID **switched_from);
VOID _tx_sb_thread_start(VOID);

/* Invoked by the VRQ entry code in vectors.S.*/
VOID _tx_sb_vrq_epilogue(VOID);

/* Tick processing.*/
VOID _tx_timer_interrupt(VOID);

/* Start of the memory not used by the application.*/
extern uint8_t __heap_base__[];

/**
 * @brief   Returns the current thread to the scheduler.
 * @note    Invoked with VRQs disabled, it returns when the thread is
 *          switched in again, with VRQs disabled.
 *
 * @param[in] thread_ptr    the current thread
 */
static VOID _tx_sb_return_to_scheduler(TX_THREAD *thread_ptr) {

  /* Saving the remaining time slice of the thread.*/
  if (_tx_timer_time_slice != 0UL) {
    thread_ptr->tx_thread_time_slice = _tx_timer_time_slice;
    _tx_timer_time_slice = 0UL;
  }

  _tx_thread_current_ptr = TX_NULL;
  _tx_sb_switch(_tx_sb_scheduler_sp, &thread_ptr->tx_thread_stack_ptr);
}

/*===========================================================================*/
/* Module VRQ handlers.                                                      */
/*===========================================================================*/

/**
 * @brief   Tick VRQ handler.
 * @details All the tick periods elapsed since the last accounted tick are
 *          processed. The host merges the alarm VRQs triggered while the
 *          sandbox is not running, those ticks are recovered here.
 */
TX_SB_VRQ_HANDLER(TX_SB_VRQ_TICK) {
  systime_t now;

  now = sbGetSystemTime();

  /* A negative difference is only possible with a host system time
     narrower than 32 bits, the elapsed ticks are lost in that case.*/
  if ((int32_t)(now - _tx_sb_last_tick_time) < 0) {
    _tx_sb_last_tick_time = now;
  }

  while ((sysinterval_t)(now - _tx_sb_last_tick_time) >=
         _tx_sb_tick_interval) {
    _tx_sb_last_tick_time += _tx_sb_tick_interval;
    _tx_timer_interrupt();
  }
}

/**
 * @brief   VRQ epilogue hook.
 * @details Returns the interrupted thread to the scheduler if a different
 *          thread has to be executed and preemption is not disabled. It is
 *          invoked by the common VRQ entry code with VRQs disabled. If no
 *          thread was running then the VRQ interrupted the scheduler, it
 *          checks for a thread to execute on return.
 */
VOID _tx_sb_vrq_epilogue(VOID) {
  TX_THREAD *thread_ptr = _tx_thread_current_ptr;

  if ((thread_ptr != TX_NULL) &&
      (thread_ptr != _tx_thread_execute_ptr) &&
      (_tx_thread_preempt_disable == 0U)) {
    _tx_sb_return_to_scheduler(thread_ptr);
  }
}

/*===========================================================================*/
/* Module exported functions.                                                */
/*===========================================================================*/

/**
 * @brief   Low level initialization.
 * @details The first unused memory address passed to
 *          @p tx_application_define() is the start of the heap region, it
 *          is also used by the C library allocator. The tick is a periodic
 *          host alarm, the tick period is rounded down to the host time
 *          resolution, the first tick is served when the first thread
 *          starts.
 */
VOID _tx_initialize_low_level(VOID) {

  _tx_initialize_unused_memory = (VOID *)__heap_base__;

  _tx_sb_tick_interval = sbGetFrequency() /
                         (sysinterval_t)TX_TIMER_TICKS_PER_SECOND;
  if (_tx_sb_tick_interval == (sysinterval_t)0) {
    /* Tick rate exceeding the host system time frequency.*/
    sbExit((msg_t)0xA9000000U);
  }

  _tx_sb_last_tick_time = sbGetSystemTime();
  (void)__sb_vrq_seten(1UL << TX_SB_VRQ_TICK);
  (void)sbSetAlarm(_tx_sb_tick_interval, true);
}

/**
 * @brief   Scheduler.
 * @details Executes the thread in @p _tx_thread_execute_ptr, the CPU is
 *          released to the host while there is no thread to execute.
 *          Threads return here, with VRQs disabled, when they suspend or
 *          are preempted.
 * @note    It never returns.
 */
VOID _tx_thread_schedule(VOID) {
  TX_THREAD *thread_ptr;

  __sb_vrq_disable();

  for (;;) {
    thread_ptr = _tx_thread_execute_ptr;
    if (thread_ptr == TX_NULL) {
      /* Waiting for a VRQ, then serving it, VRQ handlers can make a thread
         ready.*/
      __sb_vrq_wait();
      __sb_vrq_enable();
      __sb_vrq_disable();
      continue;
    }

    _tx_thread_current_ptr = thread_ptr;
    thread_ptr->tx_thread_run_count++;
    _tx_timer_time_slice = thread_ptr->tx_thread_time_slice;

    _tx_sb_switch(thread_ptr->tx_thread_stack_ptr, &_tx_sb_scheduler_sp);
  }
}

/**
 * @brief   Returns the current thread to the scheduler.
 * @details Invoked by the kernel when the current thread suspends or a
 *          different thread has to be executed, it returns when the thread
 *          is executed again.
 */
VOID _tx_thread_system_return(VOID) {
  UINT sts;

  sts = _tx_sb_vrq_lock();
  _tx_sb_return_to_scheduler(_tx_thread_current_ptr);
  _tx_sb_vrq_unlock(sts);
}

/**
 * @brief   Builds the initial context of a thread.
 * @details The context is placed at the 8 bytes aligned top of the thread
 *          stack, the thread starts at @p function_ptr with VRQs enabled.
 *
 * @param[in] thread_ptr    the thread
 * @param[in] function_ptr  the thread start function
 */
VOID _tx_thread_stack_build(TX_THREAD *thread_ptr, VOID (*function_ptr)(VOID)) {
  struct port_switch_frame *fp;
  uintptr_t top;

  top = ((uintptr_t)thread_ptr->tx_thread_stack_end + 1U) &
        ~(uintptr_t)7U;
  fp = (struct port_switch_frame *)top - 1;
  (void)memset(fp, 0, sizeof (*fp));
  fp->fpscr = TX_SB_INITIAL_FPSCR;
  fp->r4 = (uint32_t)function_ptr;
  fp->lr = (uint32_t)_tx_sb_thread_start;

  thread_ptr->tx_thread_stack_ptr = (VOID *)fp;
}

/**
 * @brief   Changes the interrupt posture.
 *
 * @param[in] new_posture   @p TX_INT_DISABLE or @p TX_INT_ENABLE
 * @return                  The previous posture.
 */
UINT _tx_thread_interrupt_control(UINT new_posture) {
  UINT old_posture;

  old_posture = (UINT)__sb_vrq_getisr() & TX_INT_DISABLE;
  if (new_posture == TX_INT_DISABLE) {
    if (old_posture == TX_INT_ENABLE) {
      __sb_vrq_disable();
    }
  }
  else {
    if (old_posture == TX_INT_DISABLE) {
      __sb_vrq_enable();
    }
  }

  return old_posture;
}

/**
 * @brief   Processes a tick.
 * @details Updates the system clock and the time slice, then processes the
 *          expired timers and the expired time slice.
 * @note    Invoked from the tick VRQ handler.
 */
VOID _tx_timer_interrupt(VOID) {

  _tx_timer_system_clock++;

  if (_tx_timer_time_slice != 0UL) {
    _tx_timer_time_slice--;
    if (_tx_timer_time_slice == 0UL) {
      _tx_timer_expired_time_slice = TX_TRUE;
    }
  }

  if (*_tx_timer_current_ptr != TX_NULL) {
    _tx_timer_expired = TX_TRUE;
  }
  else {
    _tx_timer_current_ptr++;
    if (_tx_timer_current_ptr == _tx_timer_list_end) {
      _tx_timer_current_ptr = _tx_timer_list_start;
    }
  }

  if (_tx_timer_expired != TX_FALSE) {
    _tx_timer_expiration_process();
  }

  if (_tx_timer_expired_time_slice != TX_FALSE) {
    _tx_thread_time_slice();
  }
}

/** @} */
