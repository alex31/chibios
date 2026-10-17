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
 * @file    tx_user.h
 * @brief   ThreadX configuration for a sandbox guest.
 */

#ifndef TX_USER_H
#define TX_USER_H

/* Tick rate, it must not exceed the host system time frequency.*/
#define TX_TIMER_TICKS_PER_SECOND               1000UL

/* Number of priorities.*/
#define TX_MAX_PRIORITIES                       32

/* Thread stacks are checked on thread switches, an overflow invokes the
   handler registered with tx_thread_stack_error_notify().*/
#define TX_ENABLE_STACK_CHECKING

#endif /* TX_USER_H */
