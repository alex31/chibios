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
 * @file    freertos_bindings/portable/GCC/ARMvx-M-SB/portasm.S
 * @brief   FreeRTOS ARMvx-M-SB port low level code.
 *
 * @addtogroup FREERTOS_ARMVXM_SB
 * @{
 */

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
 * Restores the context of the first task, the current stack is
 * abandoned.
 *--------------------------------------------------------------------------*/
                .thumb_func
                .globl  vPortStartFirstTask
vPortStartFirstTask:
                ldr     r0, =pxCurrentTCB
                ldr     r0, [r0]
                ldr     r0, [r0]
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
                svc     #125                /* SB_FASTC_VRQ_ENABLE */

                mov     r0, r5
                mov     lr, r6
                bx      r4

#endif /* !defined(__DOXYGEN__) */

/** @} */
