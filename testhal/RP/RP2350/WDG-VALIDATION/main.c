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
 * RP2350 WDG (watchdog) validation test.
 *
 * Validates the WDG LLD timeout arithmetic and the reset path:
 *
 *   - Clamp test: an rlr value whose microsecond scaling overflows
 *     32-bit arithmetic must be clamped to the 24-bit LOAD ceiling,
 *     not wrapped to a tiny value (which would cause an immediate
 *     spurious reset).  Verified by reading back the CTRL.TIME
 *     countdown and by petting the watchdog over a 3 second window.
 *   - Reset test: a short 100ms timeout without petting must trigger
 *     a hardware watchdog reset with REASON.TIMER set.
 *
 * The test is self-sequencing across the intentional watchdog reset:
 * WATCHDOG->SCRATCH[7] (preserved across watchdog resets) carries a
 * phase marker.
 *
 * Note: CH_DBG_ENABLE_ASSERTS is disabled in this project because the
 * clamp test deliberately requests an out-of-range interval, which
 * would trip the LLD's osalDbgAssert() in debug builds.
 *
 * Single-core only.  Output on SIOD0 (GPIO0/GPIO1) at default 38400.
 */

#include "ch.h"
#include "hal.h"
#include "chprintf.h"

#define LED_PIN              25U
#define UART_TX_PIN          0U
#define UART_RX_PIN          1U

/* Phase markers stored in WATCHDOG->SCRATCH[7], preserved across
   watchdog resets.*/
#define PHASE_MAGIC1         0x57444731U  /* Reset test armed.        */
#define PHASE_MAGIC2         0x57444732U  /* Test sequence completed. */

/* Overflowing intervals: rlr is in milliseconds, the LLD scales by
   1000 to microseconds.  Both values overflow 32-bit arithmetic:
   5000000 * 1000 wraps to 0x2A05F200 and 4294968 * 1000 wraps to
   just 704, which with the old 32-bit arithmetic would have been
   written to LOAD and caused an immediate spurious reset.*/
#define RLR_OVERFLOW_LARGE   5000000U
#define RLR_OVERFLOW_TINY    4294968U

/* A clamped countdown must start near the 24-bit ceiling; anything
   above half scale proves it was clamped rather than wrapped.*/
#define TIME_CLAMP_FLOOR     0x800000U

static BaseSequentialStream *chp;
static unsigned pass_count;
static unsigned fail_count;

/* Reset state captured at entry, before any watchdog manipulation.*/
static uint32_t boot_reason;
static uint32_t boot_scratch[8];

static const WDGConfig wdgcfg_clamp_large = {
  .rlr = RLR_OVERFLOW_LARGE
};

static const WDGConfig wdgcfg_clamp_tiny = {
  .rlr = RLR_OVERFLOW_TINY
};

static const WDGConfig wdgcfg_short = {
  .rlr = 100U
};

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
 * Starts the watchdog with the given configuration and verifies that
 * the countdown read back from CTRL.TIME is clamped near the 24-bit
 * ceiling instead of wrapped to a small value.
 */
static bool test_clamp(const WDGConfig *cfgp) {
  uint32_t count;

  wdgStart(&WDGD1, cfgp);
  count = WATCHDOG->CTRL & WATCHDOG_CTRL_TIME;

  chprintf(chp, "    rlr %u ms -> countdown 0x%06X\r\n",
           cfgp->rlr, count);

  return count > TIME_CLAMP_FLOOR;
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
/* Test phases.                                                              */
/*===========================================================================*/

/*
 * Phase 0: clamp test, pet window, then arm a short timeout and stop
 * petting so the watchdog resets the chip.
 */
static void phase0(void) {
  bool ok;
  unsigned i;

  chprintf(chp, "  Phase 0: clamp test\r\n");

  /* Overflowing interval, wrapped value still above the ceiling.*/
  ok = test_clamp(&wdgcfg_clamp_large);
  report("countdown clamped for rlr=5000000", ok);

  /* Overflowing interval, wrapped value would be a tiny 704us LOAD
     with the old 32-bit arithmetic.*/
  ok = test_clamp(&wdgcfg_clamp_tiny);
  report("countdown clamped for rlr=4294968", ok);

  /* Pet the watchdog over 3 seconds.  With the old bug a tiny LOAD
     would already have reset the chip before reaching this point.*/
  chprintf(chp, "  Petting watchdog for 3 seconds...\r\n");
  for (i = 0U; i < 12U; i++) {
    chThdSleepMilliseconds(250);
    wdgReset(&WDGD1);
  }
  report("no spurious reset during 3s pet window", true);

  /* Arm the reset test: marker first, then a short timeout with no
     further petting.  Expect a watchdog reset in ~100-200 ms.*/
  /* Phase-0 results live in BSS and would be lost across the reset,
     persisting them in scratch registers for the phase-1 summary.*/
  WATCHDOG->SCRATCH[5] = pass_count;
  WATCHDOG->SCRATCH[6] = fail_count;
  WATCHDOG->SCRATCH[7] = PHASE_MAGIC1;
  chprintf(chp, "  Arming 100 ms watchdog, expecting reset...\r\n");

  /* Let the UART drain before the reset hits.*/
  chThdSleepMilliseconds(50);

  wdgStart(&WDGD1, &wdgcfg_short);
  while (true) {
    chThdSleepMilliseconds(10);
  }
}

/*
 * Phase 1: back from the intentional watchdog reset, verify REASON.
 */
static void phase1(void) {
  bool ok;

  chprintf(chp, "  Phase 1: back from watchdog reset\r\n");

  ok = (boot_reason & WATCHDOG_REASON_TIMER) != 0U;
  if (ok) {
    chprintf(chp, "  [PASS] wdg reset observed\r\n");
    pass_count++;
  }
  else {
    report("REASON.TIMER set after watchdog reset", false);
  }

  WATCHDOG->SCRATCH[7] = PHASE_MAGIC2;

  chprintf(chp, "\r\n========================================\r\n");
  chprintf(chp, "  Results: %u pass, %u fail\r\n", pass_count, fail_count);
  if (fail_count == 0U) {
    chprintf(chp, "  ALL TESTS PASSED\r\n");
  }
  else {
    chprintf(chp, "  *** FAILURES DETECTED ***\r\n");
  }
  chprintf(chp, "========================================\r\n");

  /* Idle with the blinker running, watchdog left disabled.*/
  while (true) {
    chThdSleepMilliseconds(1000);
  }
}

/*===========================================================================*/
/* Main.                                                                     */
/*===========================================================================*/

int main(void) {
  unsigned i;

  /* Capture reset state first, before any watchdog manipulation.*/
  boot_reason = WATCHDOG->REASON;
  for (i = 0U; i < 8U; i++) {
    boot_scratch[i] = WATCHDOG->SCRATCH[i];
  }

  halInit();
  chSysInit();

  /* LED. */
  palSetLineMode(LED_PIN, PAL_MODE_OUTPUT_PUSHPULL | PAL_RP_PAD_DRIVE12);

  /* UART on GPIO0/GPIO1. */
  palSetLineMode(UART_TX_PIN, PAL_MODE_ALTERNATE_UART);
  palSetLineMode(UART_RX_PIN, PAL_MODE_ALTERNATE_UART);
  sioStart(&SIOD0, NULL);
  chp = (BaseSequentialStream *)&SIOD0;

  /* Start blinker. */
  chThdCreateStatic(waThread1, sizeof(waThread1), NORMALPRIO, Thread1, NULL);

  /* Small delay to let UART settle. */
  chThdSleepMilliseconds(100);

  chprintf(chp, "\r\n");
  chprintf(chp, "========================================\r\n");
  chprintf(chp, "  RP2350 WDG Validation\r\n");
  chprintf(chp, "========================================\r\n");
  chprintf(chp, "  REASON  = 0x%08X%s%s\r\n", boot_reason,
           ((boot_reason & WATCHDOG_REASON_TIMER) != 0U) ? " TIMER" : "",
           ((boot_reason & WATCHDOG_REASON_FORCE) != 0U) ? " FORCE" : "");
  for (i = 0U; i < 8U; i++) {
    chprintf(chp, "  SCRATCH[%u] = 0x%08X\r\n", i, boot_scratch[i]);
  }
  chprintf(chp, "\r\n");

  if (boot_scratch[7] == PHASE_MAGIC1) {
    /* Restore the phase-0 tallies persisted before the reset.*/
    pass_count += boot_scratch[5];
    fail_count += boot_scratch[6];
    phase1();
  }

  if (boot_scratch[7] == PHASE_MAGIC2) {
    chprintf(chp, "  Previous run completed, restarting sequence.\r\n");
    WATCHDOG->SCRATCH[7] = 0U;
  }

  phase0();

  return 0;
}
