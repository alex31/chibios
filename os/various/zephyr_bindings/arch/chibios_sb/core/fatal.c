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
 * @file    arch/chibios_sb/core/fatal.c
 * @brief   ChibiOS sandbox architecture fatal errors.
 * @details A system halt terminates the sandbox, the exit code reported to
 *          the host is @p CHIBIOS_SB_EXIT_HALT ORed with the reason.
 *
 * @addtogroup ZEPHYR_CHIBIOS_SB
 * @{
 */

#include <zephyr/kernel.h>

/*===========================================================================*/
/* Module exported functions.                                                */
/*===========================================================================*/

/**
 * @brief   Halts the system terminating the sandbox.
 *
 * @param[in] reason    the fatal error reason
 */
FUNC_NORETURN void arch_system_halt(unsigned int reason) {

  chibios_sb_exit((uint32_t)CHIBIOS_SB_EXIT_HALT | (reason & 0xFFFFFFU));
}

/** @} */
