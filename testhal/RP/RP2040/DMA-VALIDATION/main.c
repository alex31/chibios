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
 * RP2040 DMA abort validation test.
 *
 * Exercises the RP2040-E13 abort workaround: a slow, timer-paced
 * memory-to-memory transfer is aborted mid-flight, after which the
 * channel must be idle (BUSY clear), no spurious completion interrupt
 * may fire for the aborted transfer, the partial destination contents
 * must be consistent, and the channel must be immediately reusable for
 * a full-speed transfer that completes exactly once with correct data.
 * The abort is repeated across many phases of the transfer.
 *
 * Results are reported on UART0 (GPIO0/GPIO1) at 115200-8-N-1.
 */

#include <string.h>

#include "ch.h"
#include "hal.h"
#include "chprintf.h"

#define LED_PIN              25U
#define UART_TX_PIN          0U
#define UART_RX_PIN          1U

#define XFER_WORDS           2048U
#define STRESS_ITERATIONS    30U

static const UARTConfig uartcfg = {
  .txend1_cb  = NULL,
  .txend2_cb  = NULL,
  .rxend_cb   = NULL,
  .rxchar_cb  = NULL,
  .rxerr_cb   = NULL,
  .timeout_cb = NULL,
  .baud       = 115200U,
  .UARTLCR_H  = UART_UARTLCR_H_WLEN_8BITS | UART_UARTLCR_H_FEN,
  .UARTCR     = 0U,
  .UARTIFLS   = UART_UARTIFLS_RXIFLSEL_1_2F | UART_UARTIFLS_TXIFLSEL_1_2E,
  .UARTDMACR  = 0U
};

static uint32_t src[XFER_WORDS];
static uint32_t dst[XFER_WORDS];

static const rp_dma_channel_t *dmachp;
static volatile uint32_t dma_callbacks;

static unsigned pass_count;
static unsigned fail_count;

static void report(const char *name, bool ok) {
  char buf[96];
  size_t n;

  if (ok) {
    pass_count++;
  }
  else {
    fail_count++;
  }
  n = (size_t)chsnprintf(buf, sizeof(buf), "%s: %s\r\n",
                         ok ? "PASS" : "FAIL", name);
  (void)uartSendTimeout(&UARTD0, &n, buf, TIME_MS2I(200));
}

static void print(const char *msg) {
  char buf[96];
  size_t n;

  n = (size_t)chsnprintf(buf, sizeof(buf), "%s", msg);
  (void)uartSendTimeout(&UARTD0, &n, buf, TIME_MS2I(200));
}

static void dma_cb(void *p, uint32_t ct) {

  (void)p;
  (void)ct;
  dma_callbacks++;
}

/* Starts a timer-paced word copy of the whole source buffer; roughly
   10000 words per second at 125 MHz clk_sys, about 200 ms total.*/
static void start_paced_transfer(void) {

  memset(dst, 0, sizeof dst);
  dmaChannelSetSourceX(dmachp, (uint32_t)src);
  dmaChannelSetDestinationX(dmachp, (uint32_t)dst);
  dmaChannelSetCounterX(dmachp, XFER_WORDS);
  dmaChannelSetModeX(dmachp, DMA_CTRL_TRIG_DATA_SIZE_WORD |
                             DMA_CTRL_TRIG_INCR_READ      |
                             DMA_CTRL_TRIG_INCR_WRITE     |
                             DMA_CTRL_TRIG_TREQ_TIMER0);
  dmaChannelEnableX(dmachp);
}

/* Number of words the aborted transfer moved before stopping, derived
   from the destination contents: every source word is nonzero and the
   write pointer increments, so the copied prefix ends at the first
   still-clear word. TRANS_COUNT is reloaded by the abort and cannot be
   used for this.*/
static uint32_t transferred_words(void) {
  uint32_t i;

  for (i = 0U; i < XFER_WORDS; i++) {
    if (dst[i] == 0U) {
      break;
    }
  }
  return i;
}

/* Verifies that exactly the first 'count' destination words match the
   source and the remainder stayed clear.*/
static bool partial_copy_consistent(uint32_t count) {
  uint32_t i;

  if (count > XFER_WORDS) {
    return false;
  }
  for (i = 0U; i < count; i++) {
    if (dst[i] != src[i]) {
      return false;
    }
  }
  for (; i < XFER_WORDS; i++) {
    if (dst[i] != 0U) {
      return false;
    }
  }
  return true;
}

int main(void) {
  uint32_t i, moved;
  bool ok;

  halInit();
  chSysInit();

  palSetLineMode(LED_PIN, PAL_MODE_OUTPUT_PUSHPULL);
  palSetLineMode(UART_TX_PIN, PAL_MODE_ALTERNATE_UART);
  palSetLineMode(UART_RX_PIN, PAL_MODE_ALTERNATE_UART);
  uartStart(&UARTD0, &uartcfg);

  print("\r\nDMA-VALIDATION start\r\n");

  for (i = 0U; i < XFER_WORDS; i++) {
    /* Nonzero by construction, the abort progress scan relies on it.*/
    src[i] = ((i + 1U) * 2654435761U) | 1U;
  }

  chSysLock();
  dmachp = dmaChannelAllocI(RP_DMA_CHANNEL_ID_ANY, 2U, dma_cb, NULL);
  chSysUnlock();
  report("channel allocated", dmachp != NULL);
  if (dmachp == NULL) {
    while (true) {
    }
  }
  dmaChannelEnableInterruptX(dmachp);

  /* Pacing timer 0: clk_sys * 1 / 12500 transfers per second. Programmed
     after the first allocation, which takes the DMA block out of reset.*/
  DMA->TIMER[0] = (1U << 16) | 12500U;

  /* A mid-flight abort: no completion callback may ever fire for it.*/
  dma_callbacks = 0U;
  start_paced_transfer();
  chThdSleepMilliseconds(50);
  report("transfer running before abort", dmaChannelIsBusyX(dmachp));
  dmaChannelAbortX(dmachp);
  report("channel idle after abort", !dmaChannelIsBusyX(dmachp));
  moved = transferred_words();
  report("abort was mid-transfer", (moved > 0U) && (moved < XFER_WORDS));
  report("partial copy consistent", partial_copy_consistent(moved));

  chThdSleepMilliseconds(100);
  report("no spurious completion after abort", dma_callbacks == 0U);

  /* The channel must be immediately reusable at full speed.*/
  memset(dst, 0, sizeof dst);
  dma_callbacks = 0U;
  dmaChannelSetSourceX(dmachp, (uint32_t)src);
  dmaChannelSetDestinationX(dmachp, (uint32_t)dst);
  dmaChannelSetCounterX(dmachp, XFER_WORDS);
  dmaChannelSetModeX(dmachp, DMA_CTRL_TRIG_DATA_SIZE_WORD |
                             DMA_CTRL_TRIG_INCR_READ      |
                             DMA_CTRL_TRIG_INCR_WRITE     |
                             DMA_CTRL_TRIG_TREQ_PERMANENT);
  dmaChannelEnableX(dmachp);
  chThdSleepMilliseconds(10);
  report("reused channel completed", memcmp(src, dst, sizeof src) == 0);
  report("exactly one completion callback", dma_callbacks == 1U);

  /* Abort stress across many transfer phases.*/
  ok = true;
  for (i = 0U; (i < STRESS_ITERATIONS) && ok; i++) {
    dma_callbacks = 0U;
    start_paced_transfer();
    chThdSleepMilliseconds(1U + (i * 6U) % 180U);
    dmaChannelAbortX(dmachp);
    ok = ok && !dmaChannelIsBusyX(dmachp);
    moved = transferred_words();
    ok = ok && (moved > 0U) && (moved < XFER_WORDS);
    ok = ok && partial_copy_consistent(moved);
    chThdSleepMilliseconds(2);
    ok = ok && (dma_callbacks == 0U);
  }
  report("abort stress iterations clean", ok);

  {
    char buf[80];
    size_t n = (size_t)chsnprintf(buf, sizeof(buf),
                                  "DMA-VALIDATION complete: %u passed, %u failed\r\n",
                                  pass_count, fail_count);
    (void)uartSendTimeout(&UARTD0, &n, buf, TIME_MS2I(200));
  }

  while (true) {
    palToggleLine(LED_PIN);
    chThdSleepMilliseconds(fail_count == 0U ? 500 : 100);
  }
}
