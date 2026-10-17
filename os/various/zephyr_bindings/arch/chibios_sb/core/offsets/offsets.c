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
 * @file    arch/chibios_sb/core/offsets/offsets.c
 * @brief   ChibiOS sandbox architecture structure offsets generation.
 *
 * @addtogroup ZEPHYR_CHIBIOS_SB
 * @{
 */

#include <gen_offset.h>
#include <zephyr/kernel.h>
#include <kernel_arch_data.h>
#include <kernel_offsets.h>

GEN_ABSOLUTE_SYM(__callee_saved_t_SIZEOF, sizeof (_callee_saved_t));
GEN_ABSOLUTE_SYM(__thread_arch_t_SIZEOF, sizeof (_thread_arch_t));

GEN_ABS_SYM_END

/** @} */
