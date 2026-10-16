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
 * @file    vserial.h
 * @brief   Mini serial driver over a VIO UART unit, header.
 *
 * @addtogroup VSERIAL
 * @{
 */

#ifndef VSERIAL_H
#define VSERIAL_H

#include "FreeRTOS.h"

/*===========================================================================*/
/* Module constants.                                                         */
/*===========================================================================*/

/*===========================================================================*/
/* Module pre-compile time settings.                                         */
/*===========================================================================*/

/**
 * @brief   VIO UART unit used by the driver.
 */
#if !defined(VSERIAL_UNIT) || defined(__DOXYGEN__)
#define VSERIAL_UNIT                        0U
#endif

/**
 * @brief   VRQ assigned to the VIO UART unit by the host.
 * @note    It is pasted into the VRQ handler name, it must be a decimal
 *          integer literal without suffixes.
 */
#if !defined(VSERIAL_VRQ) || defined(__DOXYGEN__)
#define VSERIAL_VRQ                         8
#endif

/**
 * @brief   Size of the RX buffer.
 */
#if !defined(VSERIAL_RX_BUFFER_SIZE) || defined(__DOXYGEN__)
#define VSERIAL_RX_BUFFER_SIZE              64U
#endif

/**
 * @brief   Size of the TX buffer.
 */
#if !defined(VSERIAL_TX_BUFFER_SIZE) || defined(__DOXYGEN__)
#define VSERIAL_TX_BUFFER_SIZE              64U
#endif

/*===========================================================================*/
/* Derived constants and error checks.                                       */
/*===========================================================================*/

#if (VSERIAL_VRQ == portVRQ_TICK) || (VSERIAL_VRQ >= portVRQ_NUMBER)
#error "invalid VSERIAL_VRQ value"
#endif

/*===========================================================================*/
/* Module data structures and types.                                         */
/*===========================================================================*/

/*===========================================================================*/
/* Module macros.                                                            */
/*===========================================================================*/

/*===========================================================================*/
/* External declarations.                                                    */
/*===========================================================================*/

#ifdef __cplusplus
extern "C" {
#endif
  BaseType_t xVSerialStart(void);
  size_t xVSerialRead(void *pvBuffer, size_t xSize, TickType_t xTicksToWait);
  size_t xVSerialWrite(const void *pvData, size_t xSize,
                       TickType_t xTicksToWait);
  uint32_t ulVSerialGetAndClearErrors(void);
#ifdef __cplusplus
}
#endif

/*===========================================================================*/
/* Module inline functions.                                                  */
/*===========================================================================*/

#endif /* VSERIAL_H */

/** @} */
