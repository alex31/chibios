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
 * @file    sb_test_sequence_001.c
 * @brief   Test Sequence 001 code.
 *
 * @page sb_test_sequence_001 [1] Information
 *
 * File: @ref sb_test_sequence_001.c
 *
 * <h2>Description</h2>
 * This sequence reports configuration and version information about
 * the sandbox environment.
 *
 * <h2>Test Cases</h2>
 * - @subpage sb_test_001_001
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
 * @page sb_test_001_001 [1.1] Sandbox Info
 *
 * <h2>Description</h2>
 * Sandbox-related info are reported.
 *
 * <h2>Test Steps</h2>
 * - [1.1.1] Prints the sandbox settings.
 * .
 */

static void sb_test_001_001_execute(void) {

  /* [1.1.1] Prints the sandbox settings.*/
  test_set_step(1);
  {
    test_print("--- Host time frequency:                ");
    test_printn(sbGetFrequency());
    test_println(" Hz");
    test_print("--- Sandbox tick frequency:             ");
    test_printn(CH_CFG_ST_FREQUENCY);
    test_println(" Hz");
#if defined(PORT_USE_LOCAL_SYSTICK)
    test_print("--- Sandbox local system tick:          ");
    test_println(PORT_USE_LOCAL_SYSTICK == TRUE ? "yes" : "no");
#endif
    test_print("--- Test VRQ:                           ");
    test_printn(SB_TEST_VRQ);
    test_println("");
  }
  test_end_step(1);
}

static const testcase_t sb_test_001_001 = {
  "Sandbox Info",
  NULL,
  NULL,
  sb_test_001_001_execute
};

/*===========================================================================*/
/* Exported data.                                                            */
/*===========================================================================*/

/**
 * @brief   Array of test cases.
 */
const testcase_t * const sb_test_sequence_001_array[] = {
  &sb_test_001_001,
  NULL
};

/**
 * @brief   Information.
 */
const testsequence_t sb_test_sequence_001 = {
  "Information",
  sb_test_sequence_001_array
};
