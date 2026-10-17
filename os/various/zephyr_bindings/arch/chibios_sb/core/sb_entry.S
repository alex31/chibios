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
 * @file    arch/chibios_sb/core/sb_entry.S
 * @brief   ChibiOS sandbox architecture low level code.
 * @details Sandbox header, entry point, VRQ entry point and context switch.
 *
 * @addtogroup ZEPHYR_CHIBIOS_SB
 * @{
 */

#include <zephyr/arch/chibios_sb/sb.h>

#if !defined(__DOXYGEN__)

                .syntax unified
                .thumb

/*--------------------------------------------------------------------------*
 * Sandbox header, placed at the start of the code region by the linker
 * script. The host checks the magic numbers and the header size, then it
 * starts the sandbox at the entry vector. The exit vector is not used by
 * the host.
 *--------------------------------------------------------------------------*/
                .section    .sandbox, "ax"
                .align      4
                .globl      z_chibios_sb_header
z_chibios_sb_header:
                .long       0xFE9154C0
                .long       0x0C4519EF
                .long       32
                .long       __start
                .long       z_chibios_sb_exit
                .long       z_chibios_sb_vrq
                .long       0
                .long       0

                .text

/*--------------------------------------------------------------------------*
 * Entry point. The host starts the sandbox unprivileged, with VRQs
 * disabled and with the stack pointer on the startup parameters placed at
 * the top of the data region. The startup stack is abandoned, the kernel
 * initialization runs on the interrupt stack which is otherwise unused
 * because VRQ handlers run on the thread stacks.
 *--------------------------------------------------------------------------*/
                .align      2
                .thumb_func
                .globl      __start
__start:
                ldr         r0, =z_interrupt_stacks + CONFIG_ISR_STACK_SIZE
                mov         sp, r0
                bl          z_chibios_sb_prep_c
                /* Not reached.*/

/*--------------------------------------------------------------------------*
 * Exit vector, the exit code is in R0.
 *--------------------------------------------------------------------------*/
                .align      2
                .thumb_func
                .globl      z_chibios_sb_exit
z_chibios_sb_exit:
                svc         #SB_SYSC_EXIT
.Lexitloop:     b           .Lexitloop

/*--------------------------------------------------------------------------*
 * VRQ entry point. The host invokes it with VRQs disabled and with the VRQ
 * number in R0, the VRQ frame is on the stack of the interrupted thread.
 * The stack pointer is the same when returning to the host, the handler
 * can switch to another thread, the frame is popped when the interrupted
 * thread is switched in again.
 *--------------------------------------------------------------------------*/
                .align      2
                .thumb_func
                .globl      z_chibios_sb_vrq
z_chibios_sb_vrq:
                bl          z_chibios_sb_vrq_handler
                svc         #SB_FASTC_VRQ_RETURN

/*--------------------------------------------------------------------------*
 * void z_chibios_sb_switch(void *switch_to, void **switched_from)
 * Saves the callee-saved registers on the current stack, the stack
 * pointer is the switch handle of the outgoing thread, then restores the
 * context saved at the stack pointer in R0. It must be invoked with VRQs
 * disabled. The caller-saved registers are either dead at the call site
 * or already saved in a VRQ frame. R3 is saved only in order to keep the
 * stack aligned to 8 bytes.
 *--------------------------------------------------------------------------*/
                .align      2
                .thumb_func
                .globl      z_chibios_sb_switch
z_chibios_sb_switch:
                push        {r3, r4, r5, r6, r7, r8, r9, r10, r11, lr}
#if defined(__ARM_FP)
                vpush       {s16-s31}
#endif
                /* The handle is written after saving the context.*/
                str         sp, [r1]

                /* Loading the stack pointer from a register, workaround
                   for ARM errata 752419.*/
                mov         sp, r0
#if defined(__ARM_FP)
                vpop        {s16-s31}
#endif
                pop         {r3, r4, r5, r6, r7, r8, r9, r10, r11, pc}

/*--------------------------------------------------------------------------*
 * Threads start here, from the context built by arch_new_thread(). R4
 * contains the entry function and R5-R7 its parameters. Switches are
 * performed with VRQs disabled, threads start with VRQs enabled.
 *--------------------------------------------------------------------------*/
                .align      2
                .thumb_func
                .globl      z_chibios_sb_thread_start
z_chibios_sb_thread_start:
                svc         #SB_FASTC_VRQ_ENABLE
                mov         r0, r4
                mov         r1, r5
                mov         r2, r6
                mov         r3, r7
                bl          z_thread_entry
                /* Not reached.*/

#endif /* !defined(__DOXYGEN__) */

/** @} */
