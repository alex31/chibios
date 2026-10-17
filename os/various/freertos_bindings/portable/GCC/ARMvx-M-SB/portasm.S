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
 * @file    freertos_bindings/portable/GCC/ARMvx-M-SB/portasm.S
 * @brief   FreeRTOS ARMvx-M-SB port low level code.
 *
 * @addtogroup FREERTOS_ARMVXM_SB
 * @{
 */

#define _FROM_ASM_
#include "sbsysc.h"

#if !defined(__DOXYGEN__)

                .syntax unified
                .thumb
                .text

/*--------------------------------------------------------------------------*
 * Saves the current task context, invokes vTaskSwitchContext() and
 * restores the context of the task selected in pxCurrentTCB. It must be
 * invoked with VRQs disabled. The stack pointer is saved before invoking
 * vTaskSwitchContext() so that the kernel stack checks see the current
 * value.
 *--------------------------------------------------------------------------*/
                .thumb_func
                .globl  vPortSwitchContext
vPortSwitchContext:
                /* R3 is saved only in order to keep the stack aligned.*/
                push    {r3, r4, r5, r6, r7, r8, r9, r10, r11, lr}
#if defined(__ARM_FP)
                vpush   {s16-s31}
#endif

                /* Saving the stack pointer in pxTopOfStack, the first
                   field of the TCB.*/
                ldr     r4, =pxCurrentTCB
                ldr     r0, [r4]
                str     sp, [r0]

                bl      vTaskSwitchContext

                /* Workaround for ARM errata 752419, always applied
                   because the sandboxed code could run on any core.*/
                ldr     r0, [r4]
                ldr     r0, [r0]
                mov     sp, r0

#if defined(__ARM_FP)
                vpop    {s16-s31}
#endif
                pop     {r3, r4, r5, r6, r7, r8, r9, r10, r11, pc}

/*--------------------------------------------------------------------------*
 * Saves the current context and stores its stack pointer at the address
 * in R0, then restores the context of the first task. The saved context
 * is resumed by vPortRestoreContext(), returning from this function.
 *--------------------------------------------------------------------------*/
                .thumb_func
                .globl  vPortStartFirstTask
vPortStartFirstTask:
                /* R3 is saved only in order to keep the stack aligned.*/
                push    {r3, r4, r5, r6, r7, r8, r9, r10, r11, lr}
#if defined(__ARM_FP)
                vpush   {s16-s31}
#endif
                str     sp, [r0]

                ldr     r0, =pxCurrentTCB
                ldr     r0, [r0]
                ldr     r0, [r0]
                /* Falls through.*/

/*--------------------------------------------------------------------------*
 * Restores the context saved at the stack pointer in R0, the current
 * stack is abandoned.
 *--------------------------------------------------------------------------*/
                .thumb_func
                .globl  vPortRestoreContext
vPortRestoreContext:
                mov     sp, r0

#if defined(__ARM_FP)
                vpop    {s16-s31}
#endif
                pop     {r3, r4, r5, r6, r7, r8, r9, r10, r11, pc}

/*--------------------------------------------------------------------------*
 * Tasks execution starts here. VRQs are disabled because switches are
 * performed with VRQs disabled, there is no critical zone active.
 * R4 contains the task function, R5 the task parameter and R6 the address
 * the task function returns to.
 *--------------------------------------------------------------------------*/
                .thumb_func
                .globl  vPortTaskStart
vPortTaskStart:
                svc     #SB_FASTC_VRQ_ENABLE

                mov     r0, r5
                mov     lr, r6
                bx      r4

#endif /* !defined(__DOXYGEN__) */

/** @} */
