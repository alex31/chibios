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
 * @file    RP2350/rp_flash_safety.c
 * @brief   Built-in SMP flash safety hooks.
 * @details Strong implementations of the EFL XIP hooks which park the
 *          other core in RAM for the duration of flash operations, using
 *          the RP2 SMP port lockout services.
 *
 * @addtogroup HAL_EFL
 * @{
 */

#include "hal.h"

#if (HAL_USE_EFL == TRUE) && (RP_EFL_XIP_SAFETY == RP_EFL_XIP_SAFETY_LOCKOUT)

#if !defined(CH_CFG_SMP_MODE) || (CH_CFG_SMP_MODE != TRUE)
#error "RP_EFL_XIP_SAFETY_LOCKOUT requires the RT SMP kernel (CH_CFG_SMP_MODE == TRUE)"
#endif

/**
 * @brief   Parks the other core before XIP becomes unavailable.
 */
void rpEflBeforeXipOff(void) {

  __port_flash_lockout();
}

/**
 * @brief   Releases the other core once XIP is available again.
 */
void rpEflAfterXipOn(void) {

  __port_flash_unlockout();
}

#endif /* (HAL_USE_EFL == TRUE) &&
          (RP_EFL_XIP_SAFETY == RP_EFL_XIP_SAFETY_LOCKOUT) */

/** @} */
