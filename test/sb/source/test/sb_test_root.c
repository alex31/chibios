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
 * @mainpage Test Suite Specification
 * Test suite for the ChibiOS/SB sandbox interface. The suite is
 * executed inside a sandbox running ChibiOS/RT on the sandbox port, it
 * verifies the host services from the sandbox side and measures the
 * cost of crossing the sandbox boundary.
 *
 * <h2>Test Sequences</h2>
 * - @subpage sb_test_sequence_001
 * - @subpage sb_test_sequence_002
 * - @subpage sb_test_sequence_003
 * - @subpage sb_test_sequence_004
 * .
 */

/**
 * @file    sb_test_root.c
 * @brief   Test Suite root structures code.
 */

#include "hal.h"
#include "sb_test_root.h"

#if !defined(__DOXYGEN__)

/*===========================================================================*/
/* Module exported variables.                                                */
/*===========================================================================*/

/**
 * @brief   Array of test sequences.
 */
const testsequence_t * const sb_test_suite_array[] = {
  &sb_test_sequence_001,
  &sb_test_sequence_002,
  &sb_test_sequence_003,
  &sb_test_sequence_004,
  NULL
};

/**
 * @brief   Test suite root structure.
 */
const testsuite_t sb_test_suite = {
  "ChibiOS/SB Test Suite",
  sb_test_suite_array
};

/*===========================================================================*/
/* Shared code.                                                              */
/*===========================================================================*/

/*
 * Name of the test VRQ handler.
 */
#define __SB_TEST_MK_VECTOR(n)      __sb_vector##n
#define SB_TEST_MK_VECTOR(n)        __SB_TEST_MK_VECTOR(n)

/*
 * Counter of the test VRQ activations.
 */
volatile uint32_t sb_test_vrq_count;

/*
 * Arguments received by the test VRQ handler on its last activation.
 */
volatile uint32_t sb_test_vrq_nvrq;
volatile uint32_t sb_test_vrq_flags;

/*
 * Test VRQ handler.
 */
SB_VRQ_HANDLER(SB_TEST_MK_VECTOR(SB_TEST_VRQ)) {

  CH_IRQ_PROLOGUE();

  sb_test_vrq_nvrq  = nvrq;
  sb_test_vrq_flags = flags;
  sb_test_vrq_count++;

  CH_IRQ_EPILOGUE();
}

/*
 * Delays execution until next system time tick.
 */
systime_t sb_test_wait_tick(void) {

  chThdSleep(1);
  return chVTGetSystemTime();
}

/*
 * Invokes an unassigned fastcall code.
 */
msg_t sb_test_unassigned_fastcall(void) {

  __syscall0r(SB_TEST_UNASSIGNED_FASTC);
  return (msg_t)r0;
}

/*
 * Invokes an unassigned syscall code.
 */
msg_t sb_test_unassigned_syscall(void) {

  __syscall0r(SB_TEST_UNASSIGNED_SYSC);
  return (msg_t)r0;
}

/*
 * Returns the mask of the pending VRQs without changing it.
 */
uint32_t sb_test_get_pending(void) {

  return __sb_vrq_clrwt(0U);
}

#endif /* !defined(__DOXYGEN__) */
