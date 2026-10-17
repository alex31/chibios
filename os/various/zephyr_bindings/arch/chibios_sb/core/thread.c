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
 * @file    arch/chibios_sb/core/thread.c
 * @brief   ChibiOS sandbox architecture threads creation.
 *
 * @addtogroup ZEPHYR_CHIBIOS_SB
 * @{
 */

#include <zephyr/kernel.h>
#include <kernel_internal.h>

/*===========================================================================*/
/* Module local types.                                                       */
/*===========================================================================*/

/**
 * @brief   Context saved by @p z_chibios_sb_switch().
 * @note    R3 is not part of the context, it is only saved in order to
 *          keep the stack aligned to 8 bytes.
 */
struct switch_frame {
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

BUILD_ASSERT((sizeof (struct switch_frame) % ARCH_STACK_PTR_ALIGN) == 0U,
             "misaligned switch frame");

/*===========================================================================*/
/* Module exported functions.                                                */
/*===========================================================================*/

/* Low level code in sb_entry.S.*/
void z_chibios_sb_thread_start(void);

/**
 * @brief   Initializes the context of a new thread.
 * @details The context is placed right below the initial stack pointer,
 *          the thread is started by @p z_chibios_sb_thread_start() when
 *          it is switched in for the first time.
 *
 * @param[in] thread    the thread
 * @param[in] stack     the thread stack object
 * @param[in] stack_ptr the aligned initial stack pointer
 * @param[in] entry     the thread entry function
 * @param[in] p1        first entry function parameter
 * @param[in] p2        second entry function parameter
 * @param[in] p3        third entry function parameter
 */
void arch_new_thread(struct k_thread *thread, k_thread_stack_t *stack,
                     char *stack_ptr, k_thread_entry_t entry,
                     void *p1, void *p2, void *p3) {
  struct switch_frame *fp;

  ARG_UNUSED(stack);

  fp = Z_STACK_PTR_TO_FRAME(struct switch_frame, stack_ptr);
  (void)memset(fp, 0, sizeof (*fp));
  fp->r4 = (uint32_t)entry;
  fp->r5 = (uint32_t)p1;
  fp->r6 = (uint32_t)p2;
  fp->r7 = (uint32_t)p3;
  fp->lr = (uint32_t)z_chibios_sb_thread_start;

  thread->switch_handle = (void *)fp;
}

/**
 * @brief   Disables the coprocessors of a thread, not supported.
 *
 * @param[in] thread    the thread
 * @return              The operation result.
 */
int arch_coprocessors_disable(struct k_thread *thread) {

  ARG_UNUSED(thread);

  return -ENOTSUP;
}

/** @} */
