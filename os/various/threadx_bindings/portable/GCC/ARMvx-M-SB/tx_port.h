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
 * @file    threadx_bindings/portable/GCC/ARMvx-M-SB/tx_port.h
 * @brief   ThreadX ARMvx-M-SB port definitions.
 * @details ThreadX port running as a ChibiOS sandbox guest. The guest runs
 *          unprivileged, interrupts are replaced by sandbox Virtual IRQs
 *          (VRQs) and context switches are performed in user mode.
 *
 * @addtogroup THREADX_ARMVXM_SB
 * @{
 */

#ifndef TX_PORT_H
#define TX_PORT_H

#ifdef TX_INCLUDE_USER_DEFINE_FILE
#include "tx_user.h"
#endif

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "sbuser.h"

/*===========================================================================*/
/* Module constants.                                                         */
/*===========================================================================*/

/**
 * @name    Basic types
 * @{
 */
#define VOID                            void
typedef char                            CHAR;
typedef unsigned char                   UCHAR;
typedef int                             INT;
typedef unsigned int                    UINT;
typedef long                            LONG;
typedef unsigned long                   ULONG;
typedef unsigned long long              ULONG64;
typedef short                           SHORT;
typedef unsigned short                  USHORT;
#define ULONG64_DEFINED
/** @} */

/**
 * @name    Interrupt posture values for tx_interrupt_control()
 * @note    The disabled value matches the VRQ ISR register disabled bit.
 * @{
 */
#define TX_INT_DISABLE                  1U
#define TX_INT_ENABLE                   0U
/** @} */

/**
 * @brief   Number of VRQs.
 */
#define TX_SB_VRQ_NUMBER                32U

/**
 * @brief   VRQ used by the port for the system tick.
 */
#define TX_SB_VRQ_TICK                  SB_VRQ_ALARM

/**
 * @brief   ThreadX build options specific to this port.
 */
#define TX_PORT_SPECIFIC_BUILD_OPTIONS  (0)

/*===========================================================================*/
/* Module pre-compile time settings.                                         */
/*===========================================================================*/

/**
 * @brief   Number of priorities, multiple of 32 up to 1024.
 */
#ifndef TX_MAX_PRIORITIES
#define TX_MAX_PRIORITIES               32
#endif

/**
 * @brief   Minimum thread stack size.
 * @details VRQ handlers run on the stack of the interrupted thread, the
 *          stack must accommodate the host VRQ frame, up to 108 bytes, and
 *          the deepest VRQ handler.
 */
#ifndef TX_MINIMUM_STACK
#define TX_MINIMUM_STACK                512
#endif

/**
 * @brief   Timer thread stack size.
 */
#ifndef TX_TIMER_THREAD_STACK_SIZE
#define TX_TIMER_THREAD_STACK_SIZE      1024
#endif

/**
 * @brief   Timer thread priority.
 */
#ifndef TX_TIMER_THREAD_PRIORITY
#define TX_TIMER_THREAD_PRIORITY        0
#endif

/**
 * @brief   Time source for the event trace, the host system time.
 */
#ifndef TX_TRACE_TIME_SOURCE
#define TX_TRACE_TIME_SOURCE            ((ULONG)sbGetSystemTime())
#endif

/**
 * @brief   Time source mask for the event trace.
 */
#ifndef TX_TRACE_TIME_MASK
#define TX_TRACE_TIME_MASK              0xFFFFFFFFUL
#endif

/*===========================================================================*/
/* Derived constants and error checks.                                       */
/*===========================================================================*/

#if defined(TX_EXECUTION_PROFILE_ENABLE) ||                               \
    defined(TX_ENABLE_EXECUTION_CHANGE_NOTIFY)
#error "execution profile not supported by this port"
#endif

#if defined(TX_MISRA_ENABLE)
#error "MISRA mode not supported by this port"
#endif

/*===========================================================================*/
/* Module macros.                                                            */
/*===========================================================================*/

/**
 * @brief   Inline initialization of the ThreadX components.
 */
#define TX_INLINE_INITIALIZATION

/**
 * @name    Control blocks extensions, unused
 * @{
 */
#define TX_THREAD_EXTENSION_0
#define TX_THREAD_EXTENSION_1
#define TX_THREAD_EXTENSION_2
#define TX_THREAD_EXTENSION_3
#define TX_BLOCK_POOL_EXTENSION
#define TX_BYTE_POOL_EXTENSION
#define TX_EVENT_FLAGS_GROUP_EXTENSION
#define TX_MUTEX_EXTENSION
#define TX_QUEUE_EXTENSION
#define TX_SEMAPHORE_EXTENSION
#define TX_TIMER_EXTENSION
#ifndef TX_THREAD_USER_EXTENSION
#define TX_THREAD_USER_EXTENSION
#endif
/** @} */

/**
 * @name    Objects creation and deletion extensions, unused
 * @{
 */
#define TX_THREAD_CREATE_EXTENSION(thread_ptr)
#define TX_THREAD_DELETE_EXTENSION(thread_ptr)
#define TX_THREAD_COMPLETED_EXTENSION(thread_ptr)
#define TX_THREAD_TERMINATED_EXTENSION(thread_ptr)
#define TX_BLOCK_POOL_CREATE_EXTENSION(pool_ptr)
#define TX_BYTE_POOL_CREATE_EXTENSION(pool_ptr)
#define TX_EVENT_FLAGS_GROUP_CREATE_EXTENSION(group_ptr)
#define TX_MUTEX_CREATE_EXTENSION(mutex_ptr)
#define TX_QUEUE_CREATE_EXTENSION(queue_ptr)
#define TX_SEMAPHORE_CREATE_EXTENSION(semaphore_ptr)
#define TX_TIMER_CREATE_EXTENSION(timer_ptr)
#define TX_BLOCK_POOL_DELETE_EXTENSION(pool_ptr)
#define TX_BYTE_POOL_DELETE_EXTENSION(pool_ptr)
#define TX_EVENT_FLAGS_GROUP_DELETE_EXTENSION(group_ptr)
#define TX_MUTEX_DELETE_EXTENSION(mutex_ptr)
#define TX_QUEUE_DELETE_EXTENSION(queue_ptr)
#define TX_SEMAPHORE_DELETE_EXTENSION(semaphore_ptr)
#define TX_TIMER_DELETE_EXTENSION(timer_ptr)
/** @} */

/**
 * @brief   System state, non-zero in VRQ handlers and during the
 *          initialization.
 */
#define TX_THREAD_GET_SYSTEM_STATE()                                        \
  (_tx_thread_system_state | (__sb_in_vrq ? 1UL : 0UL))

/**
 * @brief   Lowest set bit of a non-zero mask.
 * @note    The kernel invokes it without a trailing semicolon.
 */
#define TX_LOWEST_SET_BIT_CALCULATE(m, b)                                   \
  (b) = (UINT)__builtin_ctz((m));

/**
 * @name    Interrupt lock, the VRQs global disable
 * @{
 */
#define TX_INTERRUPT_SAVE_AREA          UINT interrupt_save;
#define TX_DISABLE                      interrupt_save = _tx_sb_vrq_lock();
#define TX_RESTORE                      _tx_sb_vrq_unlock(interrupt_save);
/** @} */

/**
 * @brief   VRQ handler definition.
 * @details Handlers run with VRQs disabled on the stack of the interrupted
 *          thread, they can use the ThreadX services allowed from ISRs.
 * @note    @p n must be a decimal integer literal without suffixes, or a
 *          macro expanding to one, because it is pasted into the vector
 *          name. A handler with a malformed name is never invoked.
 * @note    @p TX_SB_VRQ_TICK is used by the port.
 *
 * @param[in] n         VRQ number
 */
#define TX_SB_VRQ_HANDLER(n)            TX_SB_VRQ_HANDLER_PASTE(n)
#define TX_SB_VRQ_HANDLER_PASTE(n)      void __sb_vector##n(void)

/*===========================================================================*/
/* External declarations.                                                    */
/*===========================================================================*/

extern bool __sb_in_vrq;

#ifdef TX_THREAD_INIT
CHAR _tx_version_id[] = "ThreadX ARMvx-M-SB/GNU Version 6.5.2";
#else
extern CHAR _tx_version_id[];
#endif

#ifdef __cplusplus
extern "C" {
#endif
  UINT _tx_thread_interrupt_control(UINT new_posture);
#ifdef __cplusplus
}
#endif

/*===========================================================================*/
/* Module inline functions.                                                  */
/*===========================================================================*/

/**
 * @brief   Disables VRQs returning the previous VRQ status.
 * @note    VRQs are already disabled in VRQ handlers, in that case no
 *          further action is taken.
 *
 * @return              The previous VRQ ISR register value.
 */
static inline __attribute__((always_inline)) UINT _tx_sb_vrq_lock(void) {
  UINT sts;

  sts = (UINT)__sb_vrq_getisr();
  if ((sts & TX_INT_DISABLE) == 0U) {
    __sb_vrq_disable();
  }

  return sts;
}

/**
 * @brief   Restores the VRQ status returned by @p _tx_sb_vrq_lock().
 *
 * @param[in] sts       the VRQ ISR register value to be restored
 */
static inline __attribute__((always_inline)) void _tx_sb_vrq_unlock(UINT sts) {

  if ((sts & TX_INT_DISABLE) == 0U) {
    __sb_vrq_enable();
  }
}

/**
 * @brief   Enables a VRQ.
 * @note    @p TX_SB_VRQ_TICK is enabled by the port.
 *
 * @param[in] vrq       VRQ number
 */
static inline void tx_sb_vrq_enable(UINT vrq) {

  (void)__sb_vrq_seten(1UL << vrq);
}

/**
 * @brief   Disables a VRQ.
 *
 * @param[in] vrq       VRQ number
 */
static inline void tx_sb_vrq_disable(UINT vrq) {

  (void)__sb_vrq_clren(1UL << vrq);
}

/**
 * @brief   Triggers a VRQ on the guest itself.
 * @details The VRQ handler is invoked immediately if the VRQ is enabled and
 *          VRQs are not disabled, else the VRQ remains pending.
 *
 * @param[in] vrq       VRQ number
 */
static inline void tx_sb_vrq_trigger(UINT vrq) {

  (void)__sb_vrq_setwt(1UL << vrq);
}

/**
 * @brief   Returns and clears the flags associated to a VRQ.
 *
 * @param[in] vrq       VRQ number
 * @return              The VRQ flags.
 */
static inline ULONG tx_sb_vrq_get_flags(UINT vrq) {

  return (ULONG)__sb_vrq_gcsts((uint32_t)vrq);
}

#endif /* TX_PORT_H */

/** @} */
