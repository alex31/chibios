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

#ifndef BOARD_H
#define BOARD_H

/*===========================================================================*/
/* Driver constants.                                                         */
/*===========================================================================*/

/*
 * Setup for Raspberry Pi Pico 2 board.
 */

/*
 * Board identifier.
 */
#define BOARD_RP_PICO2_RP2350
#define BOARD_NAME                  "Raspberry Pi Pico 2"

/*
 * Board oscillators-related settings.
 */
#if !defined(RP_XOSCCLK)
#define RP_XOSCCLK                  12000000U
#endif

/*
 * MCU type.
 * The Pico 2 mounts the RP2350A (QFN-60) package and correctly defaults to
 * the A-package capabilities (GPIO0-29, AINSEL 0-3 on GPIO26-29). Boards
 * mounting the RP2350B (QFN-80) package must define RP2350B_QFN80 in their
 * board.h/board files to advertise the extended capabilities (GPIO0-47,
 * AINSEL 0-7 on GPIO40-47).
 */
#define RP2350

/*
 * IO pins assignments.
 */

/*
 * IO lines assignments.
 */

/*===========================================================================*/
/* Driver pre-compile time settings.                                         */
/*===========================================================================*/

/*===========================================================================*/
/* Derived constants and error checks.                                       */
/*===========================================================================*/

/*===========================================================================*/
/* Driver data structures and types.                                         */
/*===========================================================================*/

/*===========================================================================*/
/* Driver macros.                                                            */
/*===========================================================================*/

/*===========================================================================*/
/* External declarations.                                                    */
/*===========================================================================*/

#if !defined(_FROM_ASM_)
#ifdef __cplusplus
extern "C" {
#endif
  void boardInit(void);
#ifdef __cplusplus
}
#endif
#endif /* _FROM_ASM_ */

#endif /* BOARD_H */
