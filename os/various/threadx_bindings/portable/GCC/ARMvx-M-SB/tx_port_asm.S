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
 * @file    threadx_bindings/portable/GCC/ARMvx-M-SB/tx_port_asm.S
 * @brief   ThreadX ARMvx-M-SB port low level code.
 *
 * @addtogroup THREADX_ARMVXM_SB
 * @{
 */

#define _FROM_ASM_
#include "sbsysc.h"

#if !defined(__DOXYGEN__)

                .syntax unified
                .thumb
                .text

/*--------------------------------------------------------------------------*
 * void _tx_sb_switch(void *switch_to, void **switched_from)
 * Saves the callee-saved registers on the current stack and the stack
 * pointer at the address in R1, then restores the context saved at the
 * stack pointer in R0. It must be invoked with VRQs disabled. In FPU
 * builds FPSCR is saved in place of R3, which is otherwise saved only in
 * order to keep the stack aligned to 8 bytes.
 *--------------------------------------------------------------------------*/
                .align      2
                .thumb_func
                .globl      _tx_sb_switch
_tx_sb_switch:
#if defined(__ARM_FP)
                vmrs        r3, FPSCR
#endif
                push        {r3, r4, r5, r6, r7, r8, r9, r10, r11, lr}
#if defined(__ARM_FP)
                vpush       {s16-s31}
#endif
                str         sp, [r1]

                /* Loading the stack pointer from a register, workaround
                   for ARM errata 752419.*/
                mov         sp, r0
#if defined(__ARM_FP)
                vpop        {s16-s31}
                pop         {r3, r4, r5, r6, r7, r8, r9, r10, r11, lr}
                vmsr        FPSCR, r3
                bx          lr
#else
                pop         {r3, r4, r5, r6, r7, r8, r9, r10, r11, pc}
#endif

/*--------------------------------------------------------------------------*
 * Threads start here, from the context built by _tx_thread_stack_build().
 * R4 contains the thread start function. Threads are switched in with
 * VRQs disabled, they start with VRQs enabled.
 *--------------------------------------------------------------------------*/
                .align      2
                .thumb_func
                .globl      _tx_sb_thread_start
_tx_sb_thread_start:
                svc         #SB_FASTC_VRQ_ENABLE
                blx         r4
                /* Not reached.*/

#endif /* !defined(__DOXYGEN__) */

/** @} */
