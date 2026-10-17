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
 * @file    portab.h
 * @brief   Application portability macros and structures.
 *
 * @addtogroup application_portability
 * @{
 */

#ifndef PORTAB_H
#define PORTAB_H

/* Host messages, USART1 on PA9/PA10 (Arduino D8/D2), not connected to the
   ST-LINK.*/
#define PORTAB_SIO_HOST                     SIOD1

/* Console of the XSHELL sandbox, LPUART1 on the ST-LINK virtual COM port.*/
#define PORTAB_SIO_SB1                      LPSIOD1

/* LED of the Zephyr sandbox.*/
#define PORTAB_LED1_PORT                    GPIOA
#define PORTAB_LED1_PAD                     GPIOA_LED_GREEN

#ifdef __cplusplus
extern "C" {
#endif
  void portab_setup(void);
#ifdef __cplusplus
}
#endif

#endif /* PORTAB_H */

/** @} */
