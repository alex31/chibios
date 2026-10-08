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

/*
 * Full-speed UAC1 microphone test: mono signed 16-bit PCM, 48 kHz, 440 Hz.
 * Connect the NUCLEO-H723ZG user USB connector to the host. No microphone or
 * analog wiring is needed: samples are synthesized, paced by the USB frames.
 *
 * On Linux, find "ChibiOS HAL USB Audio" with "arecord -l", then record using
 * that card's hardware PCM, e.g.:
 *   arecord -D hw:CARD=<audio-card-id>,DEV=0 -t wav -f S16_LE -r 48000 \
 *           -c 1 -d 10 tone.wav
 * Check rate, tone frequency and continuity, then repeat open/close and USB
 * reconnect. Green LED blinks faster while configured. audio_stats is
 * available through the debugger.
 *
 * EP0 uses the default ISR-driven handling: alternate settings are selected
 * by the requests hook in source/usbaudio.c. This is deliberately a
 * single-function, IN-only test. A hardware sample clock would need rate
 * matching or USB feedback, fixed 48-sample packets cannot prevent long-term
 * buffer drift.
 */

#include "ch.h"
#include "hal.h"
#include "portab.h"

#include "usbcfg.h"

/*
 * Application entry point.
 */
int main(void) {

  /*
   * System initializations.
   * - HAL initialization, this also initializes the configured device drivers
   *   and performs the board-specific initializations.
   * - Kernel initialization, the main() function becomes a thread and the
   *   RTOS is active.
   */
  halInit();
  chSysInit();

  /*
   * Board-dependent initialization.
   */
  portab_setup();

  /*
   * Activates the USB driver and then the USB bus pull-up on D+.
   * Note, a delay is inserted in order to not have to disconnect the cable
   * after a reset.
   */
  if (usbStart(&PORTAB_USB1, &usbcfg) != HAL_RET_SUCCESS) {
    chSysHalt("USB start failed");
  }
  usbDisconnectBus(&PORTAB_USB1);
  chThdSleepMilliseconds(1500);
  usbConnectBus(&PORTAB_USB1);

  /*
   * Normal main() thread activity, the LED blinks faster while configured.
   */
  while (true) {
    palToggleLine(PORTAB_BLINK_LED1);
    chThdSleepMilliseconds(usbGetDriverStateI(&PORTAB_USB1) == USB_ACTIVE ?
                           250 : 500);
  }
}
