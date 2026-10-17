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

#include "ch.h"
#include "hal.h"

/*
 * LED line. VIO ports are local to each sandbox, IOPORT1 is the VIO GPIO
 * unit 0 of this sandbox and its mapping on a physical port is decided by
 * the host configuration. The host must assign a unit 0 with write
 * permission and the LED on its pad 0.
 */
#define LINE_LED                PAL_LINE(IOPORT1, 0U)

/*
 * Blinker thread.
 */
static THD_WORKING_AREA(waThread1, 128);
static THD_FUNCTION(Thread1, arg) {

  (void)arg;

  while (true) {
    palToggleLine(LINE_LED);
    chThdSleepMilliseconds(500);
  }
}

/*
 * Threads creation table, one entry per thread.
 */
THD_TABLE_BEGIN
  THD_TABLE_THREAD(0, "blinker",      waThread1,       Thread1,      NULL)
THD_TABLE_END

/*
 * Application entry point.
 */
int main(void) {

  /*
   * System initializations.
   * - Virtual HAL initialization.
   * - Kernel initialization, the main() function becomes a thread and the
   *   RTOS is active.
   */
  halInit();
  chSysInit();

  /* This is now the idle thread loop, you must never try to sleep or wait
     in this loop. The CPU is released to the host until a VRQ is pending,
     a busy loop would consume all the time assigned to the sandbox.*/
  while (true) {
    port_wait_for_interrupt();
  }
}
