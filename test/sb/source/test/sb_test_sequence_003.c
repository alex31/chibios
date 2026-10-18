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

#include "hal.h"
#include "sb_test_root.h"

/**
 * @file    sb_test_sequence_003.c
 * @brief   Test Sequence 003 code.
 *
 * @page sb_test_sequence_003 [3] Virtual IRQs
 *
 * File: @ref sb_test_sequence_003.c
 *
 * <h2>Description</h2>
 * This sequence tests the delivery of software-triggered VRQs to the
 * sandbox, the per-VRQ enable and the global VRQ disable are tested.
 *
 * <h2>Test Cases</h2>
 * - @subpage sb_test_003_001
 * - @subpage sb_test_003_002
 * - @subpage sb_test_003_003
 * - @subpage sb_test_003_004
 * .
 */

/*===========================================================================*/
/* Shared code.                                                              */
/*===========================================================================*/

#include "ch.h"

/*===========================================================================*/
/* Test cases.                                                               */
/*===========================================================================*/

/**
 * @page sb_test_003_001 [3.1] Software-triggered VRQ
 *
 * <h2>Description</h2>
 * The test VRQ is made pending while enabled, it is expected to be
 * served on return from the triggering fastcall.
 *
 * <h2>Test Steps</h2>
 * - [3.1.1] The test VRQ is enabled, no activation is expected without
 *   a pending request.
 * - [3.1.2] The test VRQ is made pending, the handler is expected to
 *   run once and the request to be cleared. The handler is expected to
 *   receive the VRQ number and no flags.
 * .
 */

static void sb_test_003_001_setup(void) {
  (void)__sb_vrq_clren(SB_TEST_VRQ_MASK);
  (void)__sb_vrq_clrwt(SB_TEST_VRQ_MASK);
  sb_test_vrq_count = 0U;
}

static void sb_test_003_001_teardown(void) {
  (void)__sb_vrq_clren(SB_TEST_VRQ_MASK);
  (void)__sb_vrq_clrwt(SB_TEST_VRQ_MASK);
}

static void sb_test_003_001_execute(void) {

  /* [3.1.1] The test VRQ is enabled, no activation is expected without
     a pending request.*/
  test_set_step(1);
  {
    (void)__sb_vrq_seten(SB_TEST_VRQ_MASK);
    test_assert(sb_test_vrq_count == 0U, "spurious activation");
  }
  test_end_step(1);

  /* [3.1.2] The test VRQ is made pending, the handler is expected to
     run once and the request to be cleared. The handler is expected to
     receive the VRQ number and no flags.*/
  test_set_step(2);
  {
    sb_test_vrq_nvrq  = 0xFFFFFFFFU;
    sb_test_vrq_flags = 0xFFFFFFFFU;
    (void)__sb_vrq_setwt(SB_TEST_VRQ_MASK);
    test_assert(sb_test_vrq_count == 1U, "not activated");
    test_assert((sb_test_get_pending() & SB_TEST_VRQ_MASK) == 0U,
                "still pending");
    test_assert(sb_test_vrq_nvrq == (uint32_t)SB_TEST_VRQ, "wrong VRQ number");
    test_assert(sb_test_vrq_flags == 0U, "unexpected flags");
  }
  test_end_step(2);
}

static const testcase_t sb_test_003_001 = {
  "Software-triggered VRQ",
  sb_test_003_001_setup,
  sb_test_003_001_teardown,
  sb_test_003_001_execute
};

/**
 * @page sb_test_003_002 [3.2] Per-VRQ enable
 *
 * <h2>Description</h2>
 * The test VRQ is made pending while individually disabled, it is
 * expected to be served when enabled. A request withdrawn while
 * disabled is expected to be discarded.
 *
 * <h2>Test Steps</h2>
 * - [3.2.1] The test VRQ is made pending while disabled, no activation
 *   is expected and the request is expected to stay pending.
 * - [3.2.2] The test VRQ is enabled, the pending request is expected
 *   to be served on return from the enabling fastcall.
 * - [3.2.3] The test VRQ is disabled, made pending and then withdrawn,
 *   no activation is expected after enabling it again.
 * .
 */

static void sb_test_003_002_setup(void) {
  (void)__sb_vrq_clren(SB_TEST_VRQ_MASK);
  (void)__sb_vrq_clrwt(SB_TEST_VRQ_MASK);
  sb_test_vrq_count = 0U;
}

static void sb_test_003_002_teardown(void) {
  (void)__sb_vrq_clren(SB_TEST_VRQ_MASK);
  (void)__sb_vrq_clrwt(SB_TEST_VRQ_MASK);
}

static void sb_test_003_002_execute(void) {

  /* [3.2.1] The test VRQ is made pending while disabled, no activation
     is expected and the request is expected to stay pending.*/
  test_set_step(1);
  {
    (void)__sb_vrq_setwt(SB_TEST_VRQ_MASK);
    test_assert(sb_test_vrq_count == 0U, "activated while disabled");
    test_assert((sb_test_get_pending() & SB_TEST_VRQ_MASK) != 0U,
                "not pending");
  }
  test_end_step(1);

  /* [3.2.2] The test VRQ is enabled, the pending request is expected
     to be served on return from the enabling fastcall.*/
  test_set_step(2);
  {
    (void)__sb_vrq_seten(SB_TEST_VRQ_MASK);
    test_assert(sb_test_vrq_count == 1U, "not activated");
    test_assert((sb_test_get_pending() & SB_TEST_VRQ_MASK) == 0U,
                "still pending");
  }
  test_end_step(2);

  /* [3.2.3] The test VRQ is disabled, made pending and then withdrawn,
     no activation is expected after enabling it again.*/
  test_set_step(3);
  {
    (void)__sb_vrq_clren(SB_TEST_VRQ_MASK);
    (void)__sb_vrq_setwt(SB_TEST_VRQ_MASK);
    (void)__sb_vrq_clrwt(SB_TEST_VRQ_MASK);
    (void)__sb_vrq_seten(SB_TEST_VRQ_MASK);
    test_assert(sb_test_vrq_count == 1U, "withdrawn request activated");
  }
  test_end_step(3);
}

static const testcase_t sb_test_003_002 = {
  "Per-VRQ enable",
  sb_test_003_002_setup,
  sb_test_003_002_teardown,
  sb_test_003_002_execute
};

/**
 * @page sb_test_003_003 [3.3] Global VRQ disable
 *
 * <h2>Description</h2>
 * The test VRQ is made pending inside the kernel critical zone, which
 * globally disables VRQs in the sandbox port, it is expected to be
 * served when leaving the zone.
 *
 * <h2>Test Steps</h2>
 * - [3.3.1] The test VRQ is enabled and made pending inside the
 *   critical zone, no activation is expected inside the zone.
 * - [3.3.2] The pending request is expected to have been served when
 *   leaving the critical zone.
 * .
 */

static void sb_test_003_003_setup(void) {
  (void)__sb_vrq_clren(SB_TEST_VRQ_MASK);
  (void)__sb_vrq_clrwt(SB_TEST_VRQ_MASK);
  sb_test_vrq_count = 0U;
}

static void sb_test_003_003_teardown(void) {
  (void)__sb_vrq_clren(SB_TEST_VRQ_MASK);
  (void)__sb_vrq_clrwt(SB_TEST_VRQ_MASK);
}

static void sb_test_003_003_execute(void) {
  uint32_t n;

  /* [3.3.1] The test VRQ is enabled and made pending inside the
     critical zone, no activation is expected inside the zone.*/
  test_set_step(1);
  {
    (void)__sb_vrq_seten(SB_TEST_VRQ_MASK);
    chSysLock();
    (void)__sb_vrq_setwt(SB_TEST_VRQ_MASK);
    n = sb_test_vrq_count;
    chSysUnlock();
    test_assert(n == 0U, "activated while globally disabled");
  }
  test_end_step(1);

  /* [3.3.2] The pending request is expected to have been served when
     leaving the critical zone.*/
  test_set_step(2);
  {
    test_assert(sb_test_vrq_count == 1U, "not activated");
    test_assert((sb_test_get_pending() & SB_TEST_VRQ_MASK) == 0U,
                "still pending");
  }
  test_end_step(2);
}

static const testcase_t sb_test_003_003 = {
  "Global VRQ disable",
  sb_test_003_003_setup,
  sb_test_003_003_teardown,
  sb_test_003_003_execute
};

/**
 * @page sb_test_003_004 [3.4] VRQ wait
 *
 * <h2>Description</h2>
 * A VRQ wait is performed while an enabled VRQ is pending and VRQs are
 * globally disabled, the wait is expected to return without serving
 * the request.
 *
 * <h2>Test Steps</h2>
 * - [3.4.1] The test VRQ is enabled and made pending inside the
 *   critical zone then a VRQ wait is performed, the wait is expected
 *   to return without activations.
 * - [3.4.2] The pending request is expected to have been served when
 *   leaving the critical zone.
 * .
 */

static void sb_test_003_004_setup(void) {
  (void)__sb_vrq_clren(SB_TEST_VRQ_MASK);
  (void)__sb_vrq_clrwt(SB_TEST_VRQ_MASK);
  sb_test_vrq_count = 0U;
}

static void sb_test_003_004_teardown(void) {
  (void)__sb_vrq_clren(SB_TEST_VRQ_MASK);
  (void)__sb_vrq_clrwt(SB_TEST_VRQ_MASK);
}

static void sb_test_003_004_execute(void) {
  uint32_t n;

  /* [3.4.1] The test VRQ is enabled and made pending inside the
     critical zone then a VRQ wait is performed, the wait is expected
     to return without activations.*/
  test_set_step(1);
  {
    (void)__sb_vrq_seten(SB_TEST_VRQ_MASK);
    chSysLock();
    (void)__sb_vrq_setwt(SB_TEST_VRQ_MASK);
    __sb_vrq_wait();
    n = sb_test_vrq_count;
    chSysUnlock();
    test_assert(n == 0U, "activated while globally disabled");
  }
  test_end_step(1);

  /* [3.4.2] The pending request is expected to have been served when
     leaving the critical zone.*/
  test_set_step(2);
  {
    test_assert(sb_test_vrq_count == 1U, "not activated");
  }
  test_end_step(2);
}

static const testcase_t sb_test_003_004 = {
  "VRQ wait",
  sb_test_003_004_setup,
  sb_test_003_004_teardown,
  sb_test_003_004_execute
};

/*===========================================================================*/
/* Exported data.                                                            */
/*===========================================================================*/

/**
 * @brief   Array of test cases.
 */
const testcase_t * const sb_test_sequence_003_array[] = {
  &sb_test_003_001,
  &sb_test_003_002,
  &sb_test_003_003,
  &sb_test_003_004,
  NULL
};

/**
 * @brief   Virtual IRQs.
 */
const testsequence_t sb_test_sequence_003 = {
  "Virtual IRQs",
  sb_test_sequence_003_array
};
