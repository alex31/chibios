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
 * @file    arch/chibios_sb/core/printk.c
 * @brief   ChibiOS sandbox architecture printk() output.
 * @details The output is written on the sandbox standard output, buffered
 *          by line in order to limit the number of syscalls.
 *
 * @addtogroup ZEPHYR_CHIBIOS_SB
 * @{
 */

#include <zephyr/kernel.h>

/*===========================================================================*/
/* Module local definitions.                                                 */
/*===========================================================================*/

/**
 * @brief   Sandbox standard output file descriptor.
 */
#define STDOUT_FD                       1

/**
 * @brief   Line buffer size.
 */
#define LINE_BUFFER_SIZE                64U

/*===========================================================================*/
/* Module local variables.                                                   */
/*===========================================================================*/

static char line_buffer[LINE_BUFFER_SIZE];
static size_t line_size;

/*===========================================================================*/
/* Module exported functions.                                                */
/*===========================================================================*/

/**
 * @brief   Outputs a character.
 *
 * @param[in] c         the character
 * @return              The character.
 */
int arch_printk_char_out(int c) {
  unsigned int key;

  key = arch_irq_lock();
  line_buffer[line_size++] = (char)c;
  if ((c == '\n') || (line_size >= LINE_BUFFER_SIZE)) {
    (void)chibios_sb_write(STDOUT_FD, line_buffer, line_size);
    line_size = 0U;
  }
  arch_irq_unlock(key);

  return c;
}

/** @} */
