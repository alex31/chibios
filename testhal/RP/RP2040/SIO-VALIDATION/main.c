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
 * RP2040 SIO (PL011 UART) validation test.
 *
 * UART0 is both the console and the device under test: it is the UART
 * bridged to the debug probe on a standard Pico setup.
 *
 * Covered functionality:
 *
 * 1. TX-end synchronization (sioSynchronizeTXEnd).  The PL011 has no
 *    transmission-complete interrupt, TX-end is only observable through
 *    UARTFR.BUSY.  The driver wakes TX-end waiters through a polling
 *    virtual timer plus an opportunistic check in the interrupt handler.
 *    The test writes a 256-byte pattern and measures, with the
 *    free-running 1 MHz TIMER0, the time from the first write call to
 *    sioSynchronizeTXEnd() returning - queueing time included.  At 38400
 *    bauds 256 frames of 10 bits take about 66.7 ms; the test passes when
 *    the measured time is within [60 ms, 80 ms], proving that the call
 *    returned when the wire actually drained, neither early nor late.
 *    On a driver without TX-end wakeup the call never returns: a 5 s
 *    watchdog is armed so that an unfixed run visibly resets, and the
 *    watchdog reset reason is printed at boot.  Note that the watchdog
 *    pauses while a debugger is attached.
 *
 * 2. FBRD rounding carry.  The PL011 fractional divider rounding can
 *    carry into the integer divider; the test computes, for the actual
 *    peripheral clock, a rate whose divider lands exactly on the carry
 *    case (div & 0x7F == 0x7F), reconfigures UART0 to that rate, prints
 *    a fixed token line repeatedly and then returns to 38400 bauds.  The
 *    firmware-side PASS is only that both reconfigurations succeeded;
 *    reading the token at the carry rate on the host side is a manual
 *    step for the operator.
 *
 * 3. Invalid rate rejection.  sioStart() with a 0 baud configuration
 *    must return an error instead of dividing by zero.
 *
 * 4. RX-idle synchronization (sioSynchronizeRXIdle).  The PL011 signals
 *    idle through its receive timeout: 32 bit periods without new data
 *    while the RX FIFO is not empty.  UART1 runs in internal loopback
 *    (UARTCR.LBE, no pins involved); 8 frames are written and the time
 *    from the first write to sioSynchronizeRXIdle() returning is measured
 *    with TIMER0.  8 frames plus the 32-bit timeout take 112 bit periods,
 *    about 2.9 ms at 38400 bauds; the test passes when the call returns
 *    MSG_OK within [2.5 ms, 10 ms] and the 8 frames are then read back
 *    intact.  RX must not read as idle once the first frame has arrived,
 *    and the idle state must survive the FIFO being drained.  It runs
 *    twice so that re-arming over a drained idle state is covered too.
 *    On a driver that loses the idle condition the call returns
 *    MSG_TIMEOUT after the 100 ms timeout.
 *
 * 5. RX-idle with a draining callback.  Same timing as test 4, but a
 *    UART1 callback drains the RX FIFO from the interrupt handler, as
 *    Buffered SIO does.  The callback runs after the idle waiter has
 *    been woken and before it re-checks the idle state, the call must
 *    still return MSG_OK within [2.5 ms, 10 ms] and the callback must
 *    have collected the 8 frames intact.
 *
 * Single-core only (no SMP).
 */

#include <string.h>

#include "ch.h"
#include "hal.h"
#include "chprintf.h"

#define LED_PIN              25U
#define UART_TX_PIN          0U
#define UART_RX_PIN          1U

#define TEST_BAUD            38400U
#define TXEND_TEST_LEN       256U

/* 256 frames x 10 bits at 38400 bauds is ~66.7 ms.*/
#define TXEND_MIN_US         60000U
#define TXEND_MAX_US         80000U

/* 8 frames x 10 bits plus the 32-bit receive timeout at 38400 bauds is
   ~2.9 ms.*/
#define RXIDLE_TEST_LEN      8U
#define RXIDLE_MIN_US        2500U
#define RXIDLE_MAX_US        10000U

static BaseSequentialStream *chp;
static unsigned pass_count;
static unsigned fail_count;

/*
 * Console/DUT configuration, mirrors the SIO default: 38400-8-N-1 with
 * FIFOs enabled.
 */
static const SIOConfig cfg_38400 = {
  .baud         = TEST_BAUD,
  .UARTLCR_H    = UART_UARTLCR_H_WLEN_8BITS | UART_UARTLCR_H_FEN,
  .UARTCR       = 0U,
  .UARTIFLS     = UART_UARTIFLS_RXIFLSEL_1_2F | UART_UARTIFLS_TXIFLSEL_1_2E,
  .UARTDMACR    = 0U
};

/*
 * Invalid configuration used by the rate rejection test.
 */
static const SIOConfig cfg_zero_baud = {
  .baud         = 0U,
  .UARTLCR_H    = UART_UARTLCR_H_WLEN_8BITS | UART_UARTLCR_H_FEN,
  .UARTCR       = 0U,
  .UARTIFLS     = UART_UARTIFLS_RXIFLSEL_1_2F | UART_UARTIFLS_TXIFLSEL_1_2E,
  .UARTDMACR    = 0U
};

/*
 * UART1 configuration for the RX-idle test: internal loopback, TX is
 * routed straight to RX.
 */
static const SIOConfig cfg_loopback = {
  .baud         = TEST_BAUD,
  .UARTLCR_H    = UART_UARTLCR_H_WLEN_8BITS | UART_UARTLCR_H_FEN,
  .UARTCR       = UART_UARTCR_LBE,
  .UARTIFLS     = UART_UARTIFLS_RXIFLSEL_1_2F | UART_UARTIFLS_TXIFLSEL_1_2E,
  .UARTDMACR    = 0U
};

/*
 * Watchdog configuration, 5 seconds (rlr is in milliseconds).  An
 * unfixed sioSynchronizeTXEnd() hangs forever, the watchdog turns that
 * into a visible reset.
 */
static const WDGConfig wdg_cfg = {
  .rlr          = 5000U
};

static uint8_t txbuf[TXEND_TEST_LEN];

static const uint8_t rxidle_pattern[RXIDLE_TEST_LEN] = {
  'I', 'D', 'L', 'E', 'T', 'E', 'S', 'T'
};

/* Frames collected by the draining callback of test 5.*/
static uint8_t cb_rxbuf[RXIDLE_TEST_LEN];
static volatile size_t cb_rxn;

/*===========================================================================*/
/* Test helpers.                                                             */
/*===========================================================================*/

static void report(const char *name, bool ok) {

  chprintf(chp, "  [%s] %s\r\n", ok ? "PASS" : "FAIL", name);
  if (ok) {
    pass_count++;
  }
  else {
    fail_count++;
  }
}

/*
 * Blocking write of the whole buffer through the SIO synchronization
 * API: waits for TX FIFO space then pushes as much as fits, until all
 * frames are queued in the peripheral.
 */
static bool write_fully(const uint8_t *bp, size_t n) {
  size_t done = 0U;

  while (done < n) {
    if (sioSynchronizeTX(&SIOD0, TIME_INFINITE) != MSG_OK) {
      return false;
    }
    done += sioAsyncWrite(&SIOD0, &bp[done], n - done);
  }

  return true;
}

/*
 * Finds a rate whose PL011 divider lands exactly on the FBRD rounding
 * carry case for the given peripheral clock: with div = 8 * clock / baud
 * the carry happens when ((div & 0x7F) + 1) / 2 == 64, i.e. when
 * (div & 0x7F) == 0x7F.  Rates producing an integer divider at the top
 * of the range are skipped so that the carry cannot overflow UARTIBRD.
 */
static uint32_t find_carry_baud(uint32_t clock) {
  uint32_t baud;

  for (baud = 4800U; baud <= 1000000U; baud++) {
    uint32_t div  = (8U * clock) / baud;
    uint32_t idiv = div >> 7;

    if ((idiv < 1U) || (idiv >= 0xFFFFU)) {
      continue;
    }
    if ((div & 0x7FU) == 0x7FU) {
      return baud;
    }
  }

  return 0U;
}

/*===========================================================================*/
/* Individual tests.                                                         */
/*===========================================================================*/

/*
 * Test: TX-end synchronization timing.
 */
static void test_txend_timing(void) {
  uint32_t t0, t1, elapsed;
  unsigned i;
  msg_t msg;
  bool ok;

  for (i = 0U; i < TXEND_TEST_LEN; i++) {
    txbuf[i] = (uint8_t)('!' + (i % 90U));    /* Printable pattern.*/
  }

  chprintf(chp, "  TXEND-TEST-START\r\n");

  /* Letting the marker text drain passively so that the measurement
     window contains only the 256-byte pattern.  50 ms is enough for
     more than 190 frames at 38400 bauds.*/
  chThdSleepMilliseconds(50);

  t0 = TIMER0->TIMERAWL;

  ok = write_fully(txbuf, sizeof txbuf);

  /* The headline check: on an unfixed driver this call never returns
     and the watchdog resets the board.*/
  msg = sioSynchronizeTXEnd(&SIOD0, TIME_INFINITE);

  t1 = TIMER0->TIMERAWL;
  elapsed = t1 - t0;

  wdgReset(&WDGD1);

  chprintf(chp, "\r\n  TXEND elapsed: %u us (expected %u..%u)\r\n",
           elapsed, TXEND_MIN_US, TXEND_MAX_US);

  report("TX pattern fully queued", ok);
  report("sioSynchronizeTXEnd returned MSG_OK", msg == MSG_OK);
  report("TX-end synchronized with wire drain",
         (elapsed >= TXEND_MIN_US) && (elapsed <= TXEND_MAX_US));
}

/*
 * Test: FBRD rounding carry rate round-trip.
 */
static void test_fbrd_carry(void) {
  uint32_t clock, baud;
  SIOConfig carry_cfg;
  msg_t msg1, msg2;
  unsigned i;

  clock = (uint32_t)halClockGetPointX(RP_CLK_PERI);
  baud = find_carry_baud(clock);

  if (baud == 0U) {
    chprintf(chp, "  no carry rate found for peri clock %u Hz\r\n", clock);
    report("FBRD carry rate found", false);
    return;
  }

  chprintf(chp, "  CARRY-BAUD %u (peri clock %u Hz)\r\n", baud, clock);

  /* Draining the console before switching rate.*/
  (void) sioSynchronizeTXEnd(&SIOD0, TIME_INFINITE);

  carry_cfg = cfg_38400;
  carry_cfg.baud = baud;

  sioStop(&SIOD0);
  msg1 = sioStart(&SIOD0, &carry_cfg);

  if (msg1 == MSG_OK) {
    /* Emitted at the carry rate.  The host-side check (reading this
       token line at the announced rate) is performed manually by the
       operator; the firmware PASS for this leg is only that the
       reconfiguration round-trip succeeded.*/
    for (i = 0U; i < 20U; i++) {
      chprintf(chp, "FBRD-CARRY-TOKEN-OK\r\n");
      chThdSleepMilliseconds(20);
      wdgReset(&WDGD1);
    }
    (void) sioSynchronizeTXEnd(&SIOD0, TIME_INFINITE);
  }

  /* Back to the console rate.*/
  sioStop(&SIOD0);
  msg2 = sioStart(&SIOD0, &cfg_38400);

  wdgReset(&WDGD1);

  chprintf(chp, "\r\n  CARRY-TEST-DONE, back at %u\r\n", TEST_BAUD);
  report("start at FBRD carry rate", msg1 == MSG_OK);
  report("restart at 38400 bauds", msg2 == MSG_OK);
}

/*
 * Test: zero rate rejection.
 */
static void test_zero_baud_rejected(void) {
  msg_t msg, msg2;

  chprintf(chp, "  testing 0 baud rejection...\r\n");
  (void) sioSynchronizeTXEnd(&SIOD0, TIME_INFINITE);

  /* Must fail with HAL_RET_CONFIG_ERROR instead of dividing by zero.*/
  sioStop(&SIOD0);
  msg = sioStart(&SIOD0, &cfg_zero_baud);

  /* Recovering the console.*/
  msg2 = sioStart(&SIOD0, &cfg_38400);

  wdgReset(&WDGD1);

  report("0 baud configuration rejected", msg == HAL_RET_CONFIG_ERROR);
  report("console recovered after rejection", msg2 == MSG_OK);
}

/*
 * Test: RX-idle synchronization timing, UART1 in loopback.
 */
static void test_rxidle_loopback(void) {
  uint8_t rxbuf[RXIDLE_TEST_LEN];
  unsigned pass;

  if (sioStart(&SIOD1, &cfg_loopback) != MSG_OK) {
    report("UART1 loopback started", false);
    return;
  }

  for (pass = 1U; pass <= 2U; pass++) {
    uint32_t t0, t1, elapsed;
    size_t n, sent;
    bool busy, drained_idle;
    msg_t msg;

    t0 = TIMER0->TIMERAWL;
    sent = sioAsyncWrite(&SIOD1, rxidle_pattern, sizeof rxidle_pattern);

    /* Waiting for the first frame, from then on the line is busy and the
       idle state latched by the previous pass no longer holds.*/
    while (sioIsRXEmptyX(&SIOD1) &&
           ((uint32_t)(TIMER0->TIMERAWL - t0) < RXIDLE_MAX_US)) {
    }
    busy = !sioIsRXIdleX(&SIOD1);

    msg = sioSynchronizeRXIdle(&SIOD1, TIME_MS2I(100));
    t1 = TIMER0->TIMERAWL;
    elapsed = t1 - t0;

    n = sioAsyncRead(&SIOD1, rxbuf, sizeof rxbuf);

    /* The idle state survives the FIFO being drained.*/
    drained_idle = sioIsRXIdleX(&SIOD1);

    wdgReset(&WDGD1);

    chprintf(chp, "  RXIDLE pass %u: msg %d, elapsed %u us (expected %u..%u)"
                  ", %u frames read\r\n",
             pass, (int)msg, elapsed, RXIDLE_MIN_US, RXIDLE_MAX_US,
             (unsigned)n);

    report("RX-idle pattern queued", sent == sizeof rxidle_pattern);
    report("RX not idle once frames arrive", busy);
    report("sioSynchronizeRXIdle returned MSG_OK", msg == MSG_OK);
    report("RX-idle synchronized with the receive timeout",
           (elapsed >= RXIDLE_MIN_US) && (elapsed <= RXIDLE_MAX_US));
    report("RX-idle frames read back intact",
           (n == sizeof rxidle_pattern) &&
           (memcmp(rxbuf, rxidle_pattern, n) == 0));
    report("RX idle survives draining the FIFO", drained_idle);
  }

  sioStop(&SIOD1);
}

/*
 * UART1 callback of test 5, drains the RX FIFO from the interrupt
 * handler as Buffered SIO does.
 */
static void rxidle_drain_cb(SIODriver *siop) {

  while (!sioIsRXEmptyX(siop)) {
    msg_t c = sioGetX(siop);

    if (cb_rxn < sizeof cb_rxbuf) {
      cb_rxbuf[cb_rxn++] = (uint8_t)c;
    }
  }
}

/*
 * Test: RX-idle synchronization with a callback draining the RX FIFO,
 * UART1 in loopback.
 */
static void test_rxidle_callback_drain(void) {
  uint32_t t0, t1, elapsed;
  size_t sent, n;
  msg_t msg;

  if (sioStart(&SIOD1, &cfg_loopback) != MSG_OK) {
    report("UART1 loopback started", false);
    return;
  }
  cb_rxn = 0U;
  sioSetCallbackX(&SIOD1, rxidle_drain_cb);

  t0 = TIMER0->TIMERAWL;
  sent = sioAsyncWrite(&SIOD1, rxidle_pattern, sizeof rxidle_pattern);
  msg = sioSynchronizeRXIdle(&SIOD1, TIME_MS2I(100));
  t1 = TIMER0->TIMERAWL;
  elapsed = t1 - t0;
  n = cb_rxn;

  sioStop(&SIOD1);

  wdgReset(&WDGD1);

  chprintf(chp, "  RXIDLE drain callback: msg %d, elapsed %u us "
                "(expected %u..%u), %u frames collected\r\n",
           (int)msg, elapsed, RXIDLE_MIN_US, RXIDLE_MAX_US, (unsigned)n);

  report("RX-idle pattern queued", sent == sizeof rxidle_pattern);
  report("sioSynchronizeRXIdle returned MSG_OK with a draining callback",
         msg == MSG_OK);
  report("RX-idle synchronized with the receive timeout",
         (elapsed >= RXIDLE_MIN_US) && (elapsed <= RXIDLE_MAX_US));
  report("RX-idle frames collected intact by the callback",
         (n == sizeof rxidle_pattern) &&
         (memcmp(cb_rxbuf, rxidle_pattern, n) == 0));
}

/*===========================================================================*/
/* Blinker thread.                                                           */
/*===========================================================================*/

static THD_WORKING_AREA(waThread1, 256);
static THD_FUNCTION(Thread1, arg) {

  (void)arg;
  chRegSetThreadName("blinker");
  while (true) {
    palToggleLine(LED_PIN);
    chThdSleepMilliseconds(500);
  }
}

/*===========================================================================*/
/* Main.                                                                     */
/*===========================================================================*/

int main(void) {
  uint32_t reason;

  halInit();
  chSysInit();

  /* Reset reason is captured before anything can disturb it.  TIMER set
     means the previous run died with the watchdog armed, e.g. an unfixed
     sioSynchronizeTXEnd() hang.*/
  reason = WATCHDOG->REASON;

  /* LED. */
  palSetLineMode(LED_PIN, PAL_MODE_OUTPUT_PUSHPULL | PAL_RP_PAD_DRIVE12);

  /* UART0 on GPIO0/GPIO1, console and device under test.*/
  palSetLineMode(UART_TX_PIN, PAL_MODE_ALTERNATE_UART);
  palSetLineMode(UART_RX_PIN, PAL_MODE_ALTERNATE_UART);
  sioStart(&SIOD0, &cfg_38400);
  chp = (BaseSequentialStream *)&SIOD0;

  /* Start blinker. */
  chThdCreateStatic(waThread1, sizeof(waThread1), NORMALPRIO, Thread1, NULL);

  /* Small delay to let UART settle. */
  chThdSleepMilliseconds(100);

  chprintf(chp, "\r\n");
  chprintf(chp, "========================================\r\n");
  chprintf(chp, "  RP2040 SIO Validation\r\n");
  chprintf(chp, "========================================\r\n");
  chprintf(chp, "  WATCHDOG->REASON = 0x%08X%s%s\r\n", reason,
           ((reason & WATCHDOG_REASON_TIMER) != 0U) ? " TIMER" : "",
           ((reason & WATCHDOG_REASON_FORCE) != 0U) ? " FORCE" : "");
  if ((reason & WATCHDOG_REASON_TIMER) != 0U) {
    chprintf(chp, "  *** previous run was reset by the watchdog ***\r\n");
  }
  chprintf(chp, "\r\n");

  /* Watchdog armed for the duration of the tests, 5 seconds.*/
  wdgStart(&WDGD1, &wdg_cfg);

  /* Test 1: TX-end synchronization timing.*/
  test_txend_timing();

  /* Test 2: FBRD rounding carry round-trip.*/
  chprintf(chp, "\r\n");
  test_fbrd_carry();

  /* Test 3: zero rate rejection.*/
  chprintf(chp, "\r\n");
  test_zero_baud_rejected();

  /* Test 4: RX-idle synchronization timing.*/
  chprintf(chp, "\r\n");
  test_rxidle_loopback();

  /* Test 5: RX-idle with a draining callback.*/
  chprintf(chp, "\r\n");
  test_rxidle_callback_drain();

  chprintf(chp, "\r\n========================================\r\n");
  chprintf(chp, "  Results: %u pass, %u fail\r\n", pass_count, fail_count);
  if (fail_count == 0U) {
    chprintf(chp, "  ALL TESTS PASSED\r\n");
  }
  else {
    chprintf(chp, "  *** FAILURES DETECTED ***\r\n");
  }
  chprintf(chp, "========================================\r\n");

  while (true) {
    wdgReset(&WDGD1);
    chThdSleepMilliseconds(500);
  }

  return 0;
}
