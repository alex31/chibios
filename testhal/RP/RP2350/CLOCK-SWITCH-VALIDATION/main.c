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
 * RP2350 runtime clock switching validation (RP_CLOCK_DYNAMIC).
 *
 * 1. Boot frequency: the FC0 frequency counter must read clk_sys at the
 *    compile-time frequency.
 * 2. Rejection: configurations violating the PLL constraints or the
 *    RP2350-E12 clk_sys/clk_usb ratio must be refused with no clock
 *    change.
 * 3. Switch down: halClockSwitchMode(&hal_clkcfg_low) must land clk_sys
 *    at 96 MHz, verified by FC0 and by the updated clock points. The
 *    console is restarted after the switch; that it still talks at the
 *    host's unchanged baud proves the SIO driver recomputed its divider
 *    from the new clk_peri.
 * 4. Flash execution: code keeps running from XIP across switches (this
 *    program lives there) and a flash-resident pattern must checksum
 *    identically at every frequency.
 * 5. Switch back and stress: return to the default configuration, an
 *    explicit-divider round trip, a gated 200 MHz overclock leg with
 *    core voltage raise/restore, then 25 low/default round trips with
 *    FC0 verification.
 *
 * Single core. Output on SIOD0 (GPIO0/GPIO1) at the default 38400.
 */

#include "ch.h"
#include "hal.h"
#include "chprintf.h"

static BaseSequentialStream *chp = (BaseSequentialStream *)&SIOD0;

static unsigned pass_count;
static unsigned fail_count;

/* Flash-resident pattern for the XIP integrity check.*/
static const uint8_t flash_pattern[64] = {
  0xC3, 0x5A, 0x0F, 0xF0, 0x81, 0x7E, 0x24, 0x42,
  0x99, 0x66, 0x11, 0xEE, 0x33, 0xCC, 0x55, 0xAA,
  0xC3, 0x5A, 0x0F, 0xF0, 0x81, 0x7E, 0x24, 0x42,
  0x99, 0x66, 0x11, 0xEE, 0x33, 0xCC, 0x55, 0xAA,
  0xC3, 0x5A, 0x0F, 0xF0, 0x81, 0x7E, 0x24, 0x42,
  0x99, 0x66, 0x11, 0xEE, 0x33, 0xCC, 0x55, 0xAA,
  0xC3, 0x5A, 0x0F, 0xF0, 0x81, 0x7E, 0x24, 0x42,
  0x99, 0x66, 0x11, 0xEE, 0x33, 0xCC, 0x55, 0xAA
};

static void report(const char *name, bool ok) {

  if (ok) {
    pass_count++;
  }
  else {
    fail_count++;
  }
  chprintf(chp, "  [%s] %s\r\n", ok ? "PASS" : "FAIL", name);
}

/*
 * Measures a clock with the FC0 frequency counter, result in kHz.
 * Returns zero on a counter timeout so callers report a failed
 * measurement instead of hanging or comparing garbage.
 */
static uint32_t fc0_measure_khz(uint32_t src) {
  uint32_t start;

  start = TIMER0->TIMERAWL;
  while ((CLOCKS->FC0.STATUS & CLOCKS_FC0_STATUS_RUNNING) != 0U) {
    if ((uint32_t)(TIMER0->TIMERAWL - start) > 100000U) {
      return 0U;
    }
  }
  CLOCKS->FC0.REF_KHZ  = RP_CLK_REF_FREQ / 1000U;
  CLOCKS->FC0.MIN_KHZ  = 0U;
  CLOCKS->FC0.MAX_KHZ  = CLOCKS_FC0_MAX_KHZ_Msk;
  CLOCKS->FC0.INTERVAL = 10U;         /* 2^10 us test interval.*/
  CLOCKS->FC0.SRC      = src;
  start = TIMER0->TIMERAWL;
  while ((CLOCKS->FC0.STATUS & CLOCKS_FC0_STATUS_DONE) == 0U) {
    if ((uint32_t)(TIMER0->TIMERAWL - start) > 100000U) {
      return 0U;
    }
  }
  return (CLOCKS->FC0.RESULT & CLOCKS_FC0_RESULT_KHZ_Msk) >>
         CLOCKS_FC0_RESULT_KHZ_Pos;
}

static bool freq_close_khz(uint32_t measured, uint32_t expected) {

  uint32_t tol = expected / 100U;     /* 1% tolerance.*/
  return (measured >= (expected - tol)) && (measured <= (expected + tol));
}

static uint32_t flash_checksum(void) {
  uint32_t sum = 0U;
  unsigned i;

  for (i = 0U; i < sizeof flash_pattern; i++) {
    sum = (sum * 31U) + flash_pattern[i];
  }
  return sum;
}

/*
 * Restarts the console so its baud divider is recomputed from the
 * current clk_peri clock point.
 */
static void console_restart(void) {

  sioStop(&SIOD0);
  sioStart(&SIOD0, NULL);
}

int main(void) {
  uint32_t khz, sum_boot, sum_low;
  bool ok;
  unsigned i;

  halInit();
  chSysInit();

  palSetLineMode(0U, PAL_MODE_ALTERNATE_UART);
  palSetLineMode(1U, PAL_MODE_ALTERNATE_UART);
  sioStart(&SIOD0, NULL);
  chThdSleepMilliseconds(100);

  chprintf(chp, "\r\n========================================\r\n");
  chprintf(chp, "  RP2350 clock switch validation\r\n");
  chprintf(chp, "========================================\r\n");

  /* 1: boot frequency.*/
  chprintf(chp, "--- Test 1: boot frequency\r\n");
  khz = fc0_measure_khz(CLOCKS_FC0_SRC_CLK_SYS);
  chprintf(chp, "  clk_sys = %u kHz\r\n", khz);
  report("boot clk_sys at compile-time frequency",
         freq_close_khz(khz, RP_CLK_SYS_FREQ / 1000U));
  sum_boot = flash_checksum();

  /* 2: rejections, the clock must not move.*/
  chprintf(chp, "--- Test 2: invalid configurations rejected\r\n");
  {
    halclkcfg_t bad = hal_clkcfg_default;

    bad.pll_sys_refdiv = 0U;
    report("zero REFDIV rejected", halClockSwitchMode(&bad) == true);

    bad = hal_clkcfg_default;
    bad.pll_sys_vco_freq = 768000000U;  /* 48 MHz output violates E12.*/
    bad.pll_sys_postdiv1 = 4U;
    bad.pll_sys_postdiv2 = 4U;
    report("E12-violating 48 MHz rejected", halClockSwitchMode(&bad) == true);

    bad = hal_clkcfg_default;
    bad.pll_sys_vco_freq = 1500000000U; /* 300 MHz would overclock.*/
    bad.pll_sys_postdiv1 = 5U;
    bad.pll_sys_postdiv2 = 1U;
    report("overclock rejected", halClockSwitchMode(&bad) == true);

    khz = fc0_measure_khz(CLOCKS_FC0_SRC_CLK_SYS);
    report("clk_sys unchanged after rejections",
           freq_close_khz(khz, RP_CLK_SYS_FREQ / 1000U));
  }

  /* 3: switch down to 96 MHz. The console TX FIFO is drained first,
     queued bytes would otherwise leave the shifter at a wrong baud
     after the switch.*/
  chThdSleepMilliseconds(20);
  ok = (halClockSwitchMode(&hal_clkcfg_low) == false);
  console_restart();
  chprintf(chp, "--- Test 3: switch to 96 MHz\r\n");
  report("switch accepted", ok);
  khz = fc0_measure_khz(CLOCKS_FC0_SRC_CLK_SYS);
  chprintf(chp, "  clk_sys = %u kHz\r\n", khz);
  report("clk_sys measures 96 MHz", freq_close_khz(khz, 96000U));
  report("clock point follows",
         halClockGetPointX(RP_CLK_SYS) == 96000000U);
  report("clk_peri point follows",
         halClockGetPointX(RP_CLK_PERI) == 96000000U);
  report("untouched points intact",
         (halClockGetPointX(RP_CLK_REF) == RP_CLK_REF_FREQ) &&
         (halClockGetPointX(RP_CLK_USB) == RP_CLK_USB_FREQ) &&
         (halClockGetPointX(RP_CLK_ADC) == RP_CLK_ADC_FREQ));
  report("console talks after restart", true);  /* Reading this proves it.*/

  /* 4: flash integrity at the new frequency.*/
  chprintf(chp, "--- Test 4: flash execution across the switch\r\n");
  sum_low = flash_checksum();
  report("flash pattern identical at 96 MHz", sum_low == sum_boot);

  /* 5: back to default and stress.*/
  chThdSleepMilliseconds(20);
  ok = (halClockSwitchMode(&hal_clkcfg_default) == false);
  console_restart();
  chprintf(chp, "--- Test 5: switch back and stress\r\n");
  report("switch back accepted", ok);
  khz = fc0_measure_khz(CLOCKS_FC0_SRC_CLK_SYS);
  chprintf(chp, "  clk_sys = %u kHz\r\n", khz);
  report("clk_sys back at compile-time frequency",
         freq_close_khz(khz, RP_CLK_SYS_FREQ / 1000U));

  /* Explicit divider round trip: switch down with a deliberately wide
     flash divider, then verify that returning with the default
     configuration restores the boot divider instead of keeping the
     wide one at the high frequency.*/
  {
    uint32_t boot_enc, enc;
    halclkcfg_t wide = hal_clkcfg_low;

    boot_enc = QMI->M0_TIMING & QMI_TIMING_CLKDIV_Msk;
    wide.qmi_clkdiv = 8U;             /* 96 MHz / 8 = 12 MHz SCK, safe.*/
    chThdSleepMilliseconds(20);
    ok = (halClockSwitchMode(&wide) == false);
    enc = QMI->M0_TIMING & QMI_TIMING_CLKDIV_Msk;
    ok = ok && (enc == 8U);
    ok = ok && (halClockSwitchMode(&hal_clkcfg_default) == false);
    enc = QMI->M0_TIMING & QMI_TIMING_CLKDIV_Msk;
    console_restart();
    report("explicit divider applied and boot divider restored",
           ok && (enc == boot_enc));
  }

#if RP_ALLOW_OVERCLOCK == TRUE
  /* Overclock leg: 200 MHz at 1.15 V with an explicit flash divider,
     then back to the rated default. Returning with a 1100 mV request
     exercises the lower-voltage-after-frequency path. */
  {
    halclkcfg_t restore = hal_clkcfg_default;

    chThdSleepMilliseconds(20);
    ok = (halClockSwitchMode(&hal_clkcfg_overclock) == false);
    khz = fc0_measure_khz(CLOCKS_FC0_SRC_CLK_SYS);
    ok = ok && freq_close_khz(khz, 200000U);
    ok = ok && (flash_checksum() == sum_boot);
    ok = ok && (((POWMAN->VREG & POWMAN_VREG_VSEL_Msk) >>
                 POWMAN_VREG_VSEL_Pos) == 0x0CU);   /* 1.15 V.*/
    restore.vreg_mv = 1100U;
    ok = ok && (halClockSwitchMode(&restore) == false);
    ok = ok && freq_close_khz(fc0_measure_khz(CLOCKS_FC0_SRC_CLK_SYS),
                              RP_CLK_SYS_FREQ / 1000U);
    ok = ok && (((POWMAN->VREG & POWMAN_VREG_VSEL_Msk) >>
                 POWMAN_VREG_VSEL_Pos) == 0x0BU);   /* 1.10 V.*/
    console_restart();
    chprintf(chp, "  overclock leg measured %u kHz\r\n", khz);
    report("200 MHz overclock round trip with voltage restore", ok);
  }
#endif

  chThdSleepMilliseconds(20);   /* Drain before the first stress switch.*/
  ok = true;
  for (i = 0U; i < 25U; i++) {
    if (halClockSwitchMode(&hal_clkcfg_low) != false) {
      ok = false;
      break;
    }
    if (!freq_close_khz(fc0_measure_khz(CLOCKS_FC0_SRC_CLK_SYS), 96000U)) {
      ok = false;
      break;
    }
    if (halClockSwitchMode(&hal_clkcfg_default) != false) {
      ok = false;
      break;
    }
    if (!freq_close_khz(fc0_measure_khz(CLOCKS_FC0_SRC_CLK_SYS),
                        RP_CLK_SYS_FREQ / 1000U)) {
      ok = false;
      break;
    }
  }
  console_restart();
  report("25 round trips with FC0 verification", ok);
  report("flash pattern identical after stress",
         flash_checksum() == sum_boot);

  chprintf(chp, "\r\n========================================\r\n");
  chprintf(chp, "  Results: %u pass, %u fail\r\n", pass_count, fail_count);
  if (fail_count == 0U) {
    chprintf(chp, "  ALL TESTS PASSED\r\n");
  }
  else {
    chprintf(chp, "  TESTS FAILED\r\n");
  }
  chprintf(chp, "========================================\r\n");

  while (true) {
    chThdSleepMilliseconds(500);
  }
}
