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
 * @file    sb_test_sequence_004.c
 * @brief   Test Sequence 004 code.
 *
 * @page sb_test_sequence_004 [4] Benchmarks
 *
 * File: @ref sb_test_sequence_004.c
 *
 * <h2>Description</h2>
 * This module implements a series of sandbox benchmarks. The
 * benchmarks measure the cost of crossing the sandbox boundary, the
 * numbers allow to discover performance regressions in the host and
 * sandbox ports.<br> The performance is calculated by measuring the
 * number of iterations after a second of continuous operations, the
 * loop overhead is included.
 *
 * <h2>Test Cases</h2>
 * - @subpage sb_test_004_001
 * - @subpage sb_test_004_002
 * - @subpage sb_test_004_003
 * - @subpage sb_test_004_004
 * - @subpage sb_test_004_005
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
 * @page sb_test_004_001 [4.1] Null fastcall performance
 *
 * <h2>Description</h2>
 * The null fastcall is invoked in a continuous loop.
 *
 * <h2>Test Steps</h2>
 * - [4.1.1] The fastcall is invoked continuously in a one-second time
 *   window.
 * - [4.1.2] Score is printed.
 * .
 */

static void sb_test_004_001_execute(void) {
  uint32_t n;

  /* [4.1.1] The fastcall is invoked continuously in a one-second time
     window.*/
  test_set_step(1);
  {
    systime_t start, end;

    n = 0;
    start = sb_test_wait_tick();
    end = chTimeAddX(start, TIME_MS2I(1000));
    do {
      (void)sbNopFastcall();
      n++;
    } while (chVTIsSystemTimeWithinX(start, end));
  }
  test_end_step(1);

  /* [4.1.2] Score is printed.*/
  test_set_step(2);
  {
    test_print("--- Score : ");
    test_printn(n);
    test_println(" fastcalls/S");
  }
  test_end_step(2);
}

static const testcase_t sb_test_004_001 = {
  "Null fastcall performance",
  NULL,
  NULL,
  sb_test_004_001_execute
};

/**
 * @page sb_test_004_002 [4.2] System time fastcall performance
 *
 * <h2>Description</h2>
 * The host system time is read in a continuous loop.
 *
 * <h2>Test Steps</h2>
 * - [4.2.1] The fastcall is invoked continuously in a one-second time
 *   window.
 * - [4.2.2] Score is printed.
 * .
 */

static void sb_test_004_002_execute(void) {
  uint32_t n;

  /* [4.2.1] The fastcall is invoked continuously in a one-second time
     window.*/
  test_set_step(1);
  {
    systime_t start, end;

    n = 0;
    start = sb_test_wait_tick();
    end = chTimeAddX(start, TIME_MS2I(1000));
    do {
      (void)sbGetSystemTime();
      n++;
    } while (chVTIsSystemTimeWithinX(start, end));
  }
  test_end_step(1);

  /* [4.2.2] Score is printed.*/
  test_set_step(2);
  {
    test_print("--- Score : ");
    test_printn(n);
    test_println(" fastcalls/S");
  }
  test_end_step(2);
}

static const testcase_t sb_test_004_002 = {
  "System time fastcall performance",
  NULL,
  NULL,
  sb_test_004_002_execute
};

/**
 * @page sb_test_004_003 [4.3] Null syscall performance
 *
 * <h2>Description</h2>
 * The null syscall is invoked in a continuous loop, each iteration
 * includes the check for pending VRQs on return from the syscall.
 *
 * <h2>Test Steps</h2>
 * - [4.3.1] The syscall is invoked continuously in a one-second time
 *   window.
 * - [4.3.2] Score is printed.
 * .
 */

static void sb_test_004_003_execute(void) {
  uint32_t n;

  /* [4.3.1] The syscall is invoked continuously in a one-second time
     window.*/
  test_set_step(1);
  {
    systime_t start, end;

    n = 0;
    start = sb_test_wait_tick();
    end = chTimeAddX(start, TIME_MS2I(1000));
    do {
      (void)sbNopSyscall();
      n++;
    } while (chVTIsSystemTimeWithinX(start, end));
  }
  test_end_step(1);

  /* [4.3.2] Score is printed.*/
  test_set_step(2);
  {
    test_print("--- Score : ");
    test_printn(n);
    test_println(" syscalls/S");
  }
  test_end_step(2);
}

static const testcase_t sb_test_004_003 = {
  "Null syscall performance",
  NULL,
  NULL,
  sb_test_004_003_execute
};

/**
 * @page sb_test_004_004 [4.4] Software-triggered VRQ performance
 *
 * <h2>Description</h2>
 * The test VRQ is made pending in a continuous loop, each iteration
 * includes the triggering fastcall, the delivery to the handler and
 * the VRQ return fastcall.
 *
 * <h2>Test Steps</h2>
 * - [4.4.1] The test VRQ is triggered continuously in a one-second
 *   time window, each trigger is expected to activate the handler
 *   once.
 * - [4.4.2] Score is printed.
 * .
 */

static void sb_test_004_004_setup(void) {
  (void)__sb_vrq_clrwt(SB_TEST_VRQ_MASK);
  sb_test_vrq_count = 0U;
  (void)__sb_vrq_seten(SB_TEST_VRQ_MASK);
}

static void sb_test_004_004_teardown(void) {
  (void)__sb_vrq_clren(SB_TEST_VRQ_MASK);
  (void)__sb_vrq_clrwt(SB_TEST_VRQ_MASK);
}

static void sb_test_004_004_execute(void) {
  uint32_t n;

  /* [4.4.1] The test VRQ is triggered continuously in a one-second
     time window, each trigger is expected to activate the handler
     once.*/
  test_set_step(1);
  {
    systime_t start, end;

    n = 0;
    start = sb_test_wait_tick();
    end = chTimeAddX(start, TIME_MS2I(1000));
    do {
      (void)__sb_vrq_setwt(SB_TEST_VRQ_MASK);
      n++;
    } while (chVTIsSystemTimeWithinX(start, end));
    test_assert(sb_test_vrq_count == n, "activations mismatch");
  }
  test_end_step(1);

  /* [4.4.2] Score is printed.*/
  test_set_step(2);
  {
    test_print("--- Score : ");
    test_printn(n);
    test_println(" VRQs/S");
  }
  test_end_step(2);
}

static const testcase_t sb_test_004_004 = {
  "Software-triggered VRQ performance",
  sb_test_004_004_setup,
  sb_test_004_004_teardown,
  sb_test_004_004_execute
};

/**
 * @page sb_test_004_005 [4.5] Critical zone performance
 *
 * <h2>Description</h2>
 * The kernel critical zone is entered and left in a continuous loop,
 * in the sandbox port each iteration includes the global VRQ disable
 * and enable fastcalls.
 *
 * <h2>Test Steps</h2>
 * - [4.5.1] The critical zone is entered and left continuously in a
 *   one-second time window.
 * - [4.5.2] Score is printed.
 * .
 */

static void sb_test_004_005_execute(void) {
  uint32_t n;

  /* [4.5.1] The critical zone is entered and left continuously in a
     one-second time window.*/
  test_set_step(1);
  {
    systime_t start, end;

    n = 0;
    start = sb_test_wait_tick();
    end = chTimeAddX(start, TIME_MS2I(1000));
    do {
      chSysLock();
      chSysUnlock();
      n++;
    } while (chVTIsSystemTimeWithinX(start, end));
  }
  test_end_step(1);

  /* [4.5.2] Score is printed.*/
  test_set_step(2);
  {
    test_print("--- Score : ");
    test_printn(n);
    test_println(" lock/unlock cycles/S");
  }
  test_end_step(2);
}

static const testcase_t sb_test_004_005 = {
  "Critical zone performance",
  NULL,
  NULL,
  sb_test_004_005_execute
};

/*===========================================================================*/
/* Exported data.                                                            */
/*===========================================================================*/

/**
 * @brief   Array of test cases.
 */
const testcase_t * const sb_test_sequence_004_array[] = {
  &sb_test_004_001,
  &sb_test_004_002,
  &sb_test_004_003,
  &sb_test_004_004,
  &sb_test_004_005,
  NULL
};

/**
 * @brief   Benchmarks.
 */
const testsequence_t sb_test_sequence_004 = {
  "Benchmarks",
  sb_test_sequence_004_array
};
