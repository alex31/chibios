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
 * @file    zephyr/arch/chibios_sb/thread.h
 * @brief   ChibiOS sandbox architecture thread structures.
 * @details The context of a switched-out thread is saved on its own stack,
 *          the thread switch handle is the saved stack pointer, nothing is
 *          kept in the thread structure.
 *
 * @addtogroup ZEPHYR_CHIBIOS_SB
 * @{
 */

#ifndef ZEPHYR_INCLUDE_ARCH_CHIBIOS_SB_THREAD_H_
#define ZEPHYR_INCLUDE_ARCH_CHIBIOS_SB_THREAD_H_

#if !defined(_ASMLANGUAGE)

#include <zephyr/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief   Callee-saved registers, saved on the thread stack.
 */
struct _callee_saved {
};

typedef struct _callee_saved _callee_saved_t;

/**
 * @brief   Architecture-specific thread data.
 */
struct _thread_arch {
};

typedef struct _thread_arch _thread_arch_t;

#ifdef __cplusplus
}
#endif

#endif /* !defined(_ASMLANGUAGE) */

#endif /* ZEPHYR_INCLUDE_ARCH_CHIBIOS_SB_THREAD_H_ */

/** @} */
