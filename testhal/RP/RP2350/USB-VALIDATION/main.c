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
 * RP2350 USB validation test.
 *
 * CDC-ACM echo firmware: every byte received on the virtual COM port is
 * echoed back unchanged. Together with host_check.py this exercises the
 * RP USBv1 low level driver: multi-packet OUT transfers (LAST buffer
 * marking), packet-multiple and short-packet-terminated transfers, large
 * IN transfers and bus reset handling.
 *
 * A heartbeat with transfer statistics is printed on SIOD0 (GPIO0 TX,
 * GPIO1 RX, 38400-8-N-1) once every 2 seconds. The Pico 2 LED toggles on
 * echo activity.
 *
 * Single-core, no SMP.
 */

#include "ch.h"
#include "hal.h"
#include "chprintf.h"

#include "usbcfg.h"

#define LED_PIN              25U
#define UART_TX_PIN          0U
#define UART_RX_PIN          1U

#define ECHO_CHUNK_SIZE      512U

static uint8_t echo_buf[ECHO_CHUNK_SIZE];

/* Total number of bytes echoed, updated by the main thread only.*/
static volatile uint32_t echo_bytes;

/*===========================================================================*/
/* Heartbeat thread.                                                         */
/*===========================================================================*/

static THD_WORKING_AREA(waHeartbeat, 512);
static THD_FUNCTION(Heartbeat, arg) {
  BaseSequentialStream *chp = (BaseSequentialStream *)&SIOD0;

  (void)arg;
  chRegSetThreadName("heartbeat");
  while (true) {
    chThdSleepMilliseconds(2000);
    chprintf(chp, "usb: bytes=%u resets=%u state=%u\r\n",
             (unsigned)echo_bytes,
             (unsigned)usb_reset_count,
             (unsigned)USBD1.state);
  }
}

/*===========================================================================*/
/* Main.                                                                     */
/*===========================================================================*/

int main(void) {

  halInit();
  chSysInit();

  /* LED. */
  palSetLineMode(LED_PIN, PAL_MODE_OUTPUT_PUSHPULL | PAL_RP_PAD_DRIVE12);
  palClearLine(LED_PIN);

  /* Heartbeat UART on GPIO0/GPIO1, default configuration (38400-8-N-1). */
  palSetLineMode(UART_TX_PIN, PAL_MODE_ALTERNATE_UART);
  palSetLineMode(UART_RX_PIN, PAL_MODE_ALTERNATE_UART);
  sioStart(&SIOD0, NULL);

  /* Initializes a serial-over-USB CDC driver. */
  sduObjectInit(&SDU1);
  sduStart(&SDU1, &serusbcfg);

  /* Activates the USB driver and then the USB bus pull-up on D+.
     Note, a delay is inserted in order to not have to disconnect the cable
     after a reset. */
  usbDisconnectBus(serusbcfg.usbp);
  chThdSleepMilliseconds(1500);
  usbStart(serusbcfg.usbp, &usbcfg);
  usbConnectBus(serusbcfg.usbp);

  /* Heartbeat/statistics thread. */
  chThdCreateStatic(waHeartbeat, sizeof(waHeartbeat), NORMALPRIO - 1,
                    Heartbeat, NULL);

  chprintf((BaseSequentialStream *)&SIOD0,
           "\r\nRP2350 USB-VALIDATION CDC echo ready\r\n");

  /* Echo loop: read up to 512-byte chunks, write them back verbatim. */
  while (true) {
    if (SDU1.config->usbp->state == USB_ACTIVE) {
      size_t n = chnReadTimeout(&SDU1, echo_buf, sizeof echo_buf,
                                TIME_MS2I(20));
      if (n > 0U) {
        chnWrite(&SDU1, echo_buf, n);
        echo_bytes += (uint32_t)n;
        palToggleLine(LED_PIN);
      }
    }
    else {
      chThdSleepMilliseconds(50);
    }
  }

  return 0;
}
