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
 * @file    zephyr/arch/chibios_sb/exception.h
 * @brief   ChibiOS sandbox architecture exception frame.
 * @note    CPU exceptions raised by the sandbox are handled by the host,
 *          which terminates the sandbox, the guest never receives an
 *          exception frame. The structure describes the basic frame of a
 *          VRQ.
 *
 * @addtogroup ZEPHYR_CHIBIOS_SB
 * @{
 */

#ifndef ZEPHYR_INCLUDE_ARCH_CHIBIOS_SB_EXCEPTION_H_
#define ZEPHYR_INCLUDE_ARCH_CHIBIOS_SB_EXCEPTION_H_

#if !defined(_ASMLANGUAGE)

#include <zephyr/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief   Basic exception frame.
 */
struct arch_esf {
  uint32_t      r0;
  uint32_t      r1;
  uint32_t      r2;
  uint32_t      r3;
  uint32_t      r12;
  uint32_t      lr;
  uint32_t      pc;
  uint32_t      xpsr;
};

#ifdef __cplusplus
}
#endif

#endif /* !defined(_ASMLANGUAGE) */

#endif /* ZEPHYR_INCLUDE_ARCH_CHIBIOS_SB_EXCEPTION_H_ */

/** @} */
