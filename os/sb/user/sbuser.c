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
 * @file    sb/user/sbuser.c
 * @brief   ARM SandBox user API code.
 *
 * @addtogroup ARM_SANDBOX_USER_API
 * @{
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "sbuser.h"

/*===========================================================================*/
/* Module local definitions.                                                 */
/*===========================================================================*/

/*===========================================================================*/
/* Module exported variables.                                                */
/*===========================================================================*/

/*===========================================================================*/
/* Module local types.                                                       */
/*===========================================================================*/

/*===========================================================================*/
/* Module local variables.                                                   */
/*===========================================================================*/

/*===========================================================================*/
/* Module local functions.                                                   */
/*===========================================================================*/

/*===========================================================================*/
/* Module exported functions.                                                */
/*===========================================================================*/

#if (SB_USER_CFG_FAST_VRQ == TRUE) || defined(__DOXYGEN__)
/**
 * @brief   VRQ fast enable.
 * @details Out-of-line version of @p __sb_vrq_fast_enable_inline(), it is
 *          used by @p __sb_vrq_enable() if @p SB_USER_CFG_FAST_VRQ_INLINE
 *          is disabled.
 * @note    It is never inlined, also with link time optimizations.
 *
 * @notapi
 */
__attribute__((noinline)) void __sb_vrq_fast_enable(void) {

  __sb_vrq_fast_enable_inline();
}
#endif

/** @} */
