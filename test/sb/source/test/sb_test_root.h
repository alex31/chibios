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
 * @file    sb_test_root.h
 * @brief   Test Suite root structures header.
 */

#ifndef SB_TEST_ROOT_H
#define SB_TEST_ROOT_H

#include "ch_test.h"

#include "sb_test_sequence_001.h"
#include "sb_test_sequence_002.h"
#include "sb_test_sequence_003.h"
#include "sb_test_sequence_004.h"

#if !defined(__DOXYGEN__)

/*===========================================================================*/
/* External declarations.                                                    */
/*===========================================================================*/

extern const testsuite_t sb_test_suite;

#ifdef __cplusplus
extern "C" {
#endif
#ifdef __cplusplus
}
#endif

/*===========================================================================*/
/* Shared definitions.                                                       */
/*===========================================================================*/

/*
 * VRQ reserved to the test suite, it must be a plain decimal number and
 * the application must not use it.
 */
#if !defined(SB_TEST_VRQ)
#define SB_TEST_VRQ                 31
#endif

#if (SB_TEST_VRQ < 1) || (SB_TEST_VRQ > 31)
#error "invalid SB_TEST_VRQ value"
#endif

/*
 * Mask of the test VRQ.
 */
#define SB_TEST_VRQ_MASK            (1U << SB_TEST_VRQ)

/*
 * Fastcall and syscall codes not assigned by the host, they are served as
 * not implemented. The fastcall code zero is never assigned.
 */
#define SB_TEST_UNASSIGNED_FASTC    0
#define SB_TEST_UNASSIGNED_SYSC     200

/*
 * Allowed delay in host system time checks.
 */
#define SB_TEST_ALLOWED_DELAY       sbTimeMS2I(2)

extern volatile uint32_t sb_test_vrq_count;
extern volatile uint32_t sb_test_vrq_nvrq;
extern volatile uint32_t sb_test_vrq_flags;

systime_t sb_test_wait_tick(void);
msg_t sb_test_unassigned_fastcall(void);
msg_t sb_test_unassigned_syscall(void);
uint32_t sb_test_get_pending(void);

#endif /* !defined(__DOXYGEN__) */

#endif /* SB_TEST_ROOT_H */
