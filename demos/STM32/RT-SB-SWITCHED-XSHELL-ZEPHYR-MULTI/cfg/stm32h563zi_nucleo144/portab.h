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

/* Host messages, shared with the console of the XSHELL sandbox.*/
#define PORTAB_SIO_HOST                     SIOD3

/* Console of the XSHELL sandbox, USART3 on the ST-LINK virtual COM port.*/
#define PORTAB_SIO_SB1                      SIOD3

/* LED of the Zephyr sandbox, the green LED1.*/
#define PORTAB_LED1_PORT                    GPIOB
#define PORTAB_LED1_PAD                     GPIOB_LED1

#ifdef __cplusplus
extern "C" {
#endif
  void portab_setup(void);
#ifdef __cplusplus
}
#endif

#endif /* PORTAB_H */

/** @} */
