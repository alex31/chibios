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
 * @file    sb_test_sequence_002.c
 * @brief   Test Sequence 002 code.
 *
 * @page sb_test_sequence_002 [2] Host services
 *
 * File: @ref sb_test_sequence_002.c
 *
 * <h2>Description</h2>
 * This sequence tests the basic host services available to the sandbox
 * through fastcalls and syscalls.
 *
 * <h2>Test Cases</h2>
 * - @subpage sb_test_002_001
 * - @subpage sb_test_002_002
 * - @subpage sb_test_002_003
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
 * @page sb_test_002_001 [2.1] Unassigned codes
 *
 * <h2>Description</h2>
 * Fastcall and syscall codes not assigned by the host are invoked, the
 * host is expected to serve them as not implemented.
 *
 * <h2>Test Steps</h2>
 * - [2.1.1] An unassigned fastcall code is invoked, CH_RET_ENOSYS is
 *   expected.
 * - [2.1.2] An unassigned syscall code is invoked, CH_RET_ENOSYS is
 *   expected.
 * .
 */

static void sb_test_002_001_execute(void) {
  msg_t msg;

  /* [2.1.1] An unassigned fastcall code is invoked, CH_RET_ENOSYS is
     expected.*/
  test_set_step(1);
  {
    msg = sb_test_unassigned_fastcall();
    test_assert(msg == CH_RET_ENOSYS, "unexpected fastcall result");
  }
  test_end_step(1);

  /* [2.1.2] An unassigned syscall code is invoked, CH_RET_ENOSYS is
     expected.*/
  test_set_step(2);
  {
    msg = sb_test_unassigned_syscall();
    test_assert(msg == CH_RET_ENOSYS, "unexpected syscall result");
  }
  test_end_step(2);
}

static const testcase_t sb_test_002_001 = {
  "Unassigned codes",
  NULL,
  NULL,
  sb_test_002_001_execute
};

/**
 * @page sb_test_002_002 [2.2] Null calls
 *
 * <h2>Description</h2>
 * The null fastcall and the null syscall are invoked, the host is
 * expected to serve them successfully.
 *
 * <h2>Test Steps</h2>
 * - [2.2.1] The null fastcall is invoked, CH_RET_SUCCESS is expected.
 * - [2.2.2] The null syscall is invoked, CH_RET_SUCCESS is expected.
 * .
 */

static void sb_test_002_002_execute(void) {
  msg_t msg;

  /* [2.2.1] The null fastcall is invoked, CH_RET_SUCCESS is
     expected.*/
  test_set_step(1);
  {
    msg = sbNopFastcall();
    test_assert(msg == CH_RET_SUCCESS, "unexpected fastcall result");
  }
  test_end_step(1);

  /* [2.2.2] The null syscall is invoked, CH_RET_SUCCESS is expected.*/
  test_set_step(2);
  {
    msg = sbNopSyscall();
    test_assert(msg == CH_RET_SUCCESS, "unexpected syscall result");
  }
  test_end_step(2);
}

static const testcase_t sb_test_002_002 = {
  "Null calls",
  NULL,
  NULL,
  sb_test_002_002_execute
};

/**
 * @page sb_test_002_003 [2.3] Host system time
 *
 * <h2>Description</h2>
 * The host system time services are tested.
 *
 * <h2>Test Steps</h2>
 * - [2.3.1] The host system time frequency is read, a non-zero value
 *   is expected.
 * - [2.3.2] The sandbox sleeps for 10 milliseconds, the host system
 *   time is expected to advance within the allowed window.
 * .
 */

static void sb_test_002_003_execute(void) {
  systime_t t0, t1;

  /* [2.3.1] The host system time frequency is read, a non-zero value
     is expected.*/
  test_set_step(1);
  {
    test_assert(sbGetFrequency() > 0U, "invalid frequency");
  }
  test_end_step(1);

  /* [2.3.2] The sandbox sleeps for 10 milliseconds, the host system
     time is expected to advance within the allowed window.*/
  test_set_step(2);
  {
    t0 = sbGetSystemTime();
    sbSleepMilliseconds(10);
    t1 = sbGetSystemTime();
    test_assert(sbTimeDiffX(t0, t1) >= sbTimeMS2I(10), "too early");
    test_assert(sbTimeDiffX(t0, t1) <= sbTimeMS2I(10) + SB_TEST_ALLOWED_DELAY,
                "too late");
  }
  test_end_step(2);
}

static const testcase_t sb_test_002_003 = {
  "Host system time",
  NULL,
  NULL,
  sb_test_002_003_execute
};

/*===========================================================================*/
/* Exported data.                                                            */
/*===========================================================================*/

/**
 * @brief   Array of test cases.
 */
const testcase_t * const sb_test_sequence_002_array[] = {
  &sb_test_002_001,
  &sb_test_002_002,
  &sb_test_002_003,
  NULL
};

/**
 * @brief   Host services.
 */
const testsequence_t sb_test_sequence_002 = {
  "Host services",
  sb_test_sequence_002_array
};
