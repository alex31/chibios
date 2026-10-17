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
 * @file    zephyr/arch/chibios_sb/arch_inlines.h
 * @brief   ChibiOS sandbox architecture inline functions.
 *
 * @addtogroup ZEPHYR_CHIBIOS_SB
 * @{
 */

#ifndef ZEPHYR_INCLUDE_ARCH_CHIBIOS_SB_ARCH_INLINES_H_
#define ZEPHYR_INCLUDE_ARCH_CHIBIOS_SB_ARCH_INLINES_H_

#include <zephyr/kernel_structs.h>

/**
 * @brief   Returns the number of CPUs, the sandbox is single core.
 *
 * @return              The number of CPUs.
 */
static ALWAYS_INLINE unsigned int arch_num_cpus(void) {

  return CONFIG_MP_MAX_NUM_CPUS;
}

#endif /* ZEPHYR_INCLUDE_ARCH_CHIBIOS_SB_ARCH_INLINES_H_ */

/** @} */
