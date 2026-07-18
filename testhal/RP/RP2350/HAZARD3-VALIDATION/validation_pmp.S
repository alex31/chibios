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

                .section .text, "ax", @progbits
                .balign 4
                .globl  validation_pmp_store
                .type   validation_pmp_store, @function

                .option push
                .option norvc
validation_pmp_store:
                sw      zero, 0(a0)
                ret
                .option pop

                .size   validation_pmp_store, . - validation_pmp_store

                .balign 4
                .globl  validation_breakpoint
                .type   validation_breakpoint, @function

                .option push
                .option norvc
validation_breakpoint:
                ebreak
                ret
                .option pop

                .size   validation_breakpoint, . - validation_breakpoint

/*
 * Exercises the tagged, thread-mode exception entry with a live t1 value.
 * t1 is caller-saved at the C ABI boundary, so the check must surround the
 * ebreak directly in assembly rather than relying on a C caller.
 */
                .balign 4
                .globl  validation_thread_breakpoint_probe
                .type   validation_thread_breakpoint_probe, @function

                .option push
                .option norvc
validation_thread_breakpoint_probe:
                li      t1, 0x51a7c3e9
                ebreak
                li      t0, 0x51a7c3e9
                bne     t1, t0, .Lthread_breakpoint_failed
                li      a0, 1
                ret

.Lthread_breakpoint_failed:
                li      a0, 0
                ret
                .option pop

                .size   validation_thread_breakpoint_probe, . - validation_thread_breakpoint_probe

/*
 * Loads distinct values into every callee-saved register, invokes a C window
 * that performs repeated thread context switches, then verifies that the port
 * restored the complete register set. The caller's original registers are
 * preserved by the surrounding Zcmp frame.
 */
                .balign 4
                .globl  validation_context_switch_probe
                .type   validation_context_switch_probe, @function
validation_context_switch_probe:
                cm.push {ra, s0-s11}, -64

                li      s0,  0x10203040
                li      s1,  0x11213141
                li      s2,  0x12223242
                li      s3,  0x13233343
                li      s4,  0x14243444
                li      s5,  0x15253545
                li      s6,  0x16263646
                li      s7,  0x17273747
                li      s8,  0x18283848
                li      s9,  0x19293949
                li      s10, 0x1a2a3a4a
                li      s11, 0x1b2b3b4b

                call    validation_context_switch_window

                li      t0, 0x10203040
                bne     s0, t0, .Lcontext_failed
                li      t0, 0x11213141
                bne     s1, t0, .Lcontext_failed
                li      t0, 0x12223242
                bne     s2, t0, .Lcontext_failed
                li      t0, 0x13233343
                bne     s3, t0, .Lcontext_failed
                li      t0, 0x14243444
                bne     s4, t0, .Lcontext_failed
                li      t0, 0x15253545
                bne     s5, t0, .Lcontext_failed
                li      t0, 0x16263646
                bne     s6, t0, .Lcontext_failed
                li      t0, 0x17273747
                bne     s7, t0, .Lcontext_failed
                li      t0, 0x18283848
                bne     s8, t0, .Lcontext_failed
                li      t0, 0x19293949
                bne     s9, t0, .Lcontext_failed
                li      t0, 0x1a2a3a4a
                bne     s10, t0, .Lcontext_failed
                li      t0, 0x1b2b3b4b
                bne     s11, t0, .Lcontext_failed

                li      a0, 1
                j       .Lcontext_done

.Lcontext_failed:
                li      a0, 0

.Lcontext_done:
                cm.popret {ra, s0-s11}, 64

                .size   validation_context_switch_probe, . - validation_context_switch_probe
