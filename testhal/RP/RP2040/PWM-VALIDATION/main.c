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
 * RP2040 PWM validation test.
 *
 * Validates the RP PWMv1 low level driver timing and stop behavior:
 *
 *   1. Period accuracy: with frequency=1MHz and period=1000 the wrap
 *      callback must run at exactly 1kHz. 1000 wraps are timed against
 *      the free-running 1MHz TIMER0 counter (TIMERAWL). A TOP
 *      off-by-one would read ~1001000us instead of 1000000us.
 *   2. Duty sanity: width=500 out of period=1000 must produce ~50%
 *      duty, verified by sampling the pin through SIO GPIO_IN.
 *   3. change_period: after pwmChangePeriod() to 500 the wrap rate
 *      must double (1000 wraps in ~500000us).
 *   4. Stop safety: 500 start/stop cycles with the stop point swept
 *      across the PWM period. After each pwmStop() no callback may
 *      fire (the unfixed driver left the slice interrupt enabled and
 *      dereferenced the NULLed config in the shared ISR). A second
 *      slice runs across the whole test to verify that stopping one
 *      slice does not break the shared vector for the others.
 *
 * Pin mapping (RP2040A, slice = (gpio >> 1) & 7, channel = gpio & 1):
 *   GPIO2 = PWM slice 1 channel A -> PWMD1 channel 0 (routed).
 *   PWMD2 (slice 2) is used callback-only, no pin routing.
 *
 * Single-core, output on SIOD0 (GPIO0/GPIO1, 38400 8N1).
 */

#include "ch.h"
#include "hal.h"
#include "chprintf.h"

#define LED_PIN              25U
#define UART_TX_PIN          0U
#define UART_RX_PIN          1U
#define PWM_PIN              2U         /* PWM slice 1 channel A.       */

#define PWM_FREQ             1000000U   /* 1MHz counter clock.          */
#define PWM_PERIOD           1000U      /* 1kHz wrap rate.              */

static BaseSequentialStream *chp;
static unsigned pass_count;
static unsigned fail_count;

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

static uint32_t time_us(void) {

  return TIMER0->TIMERAWL;
}

static void delay_us(uint32_t us) {
  uint32_t t0 = time_us();

  while ((uint32_t)(time_us() - t0) < us) {
  }
}

/*===========================================================================*/
/* Wrap measurement machinery (PWMD1).                                       */
/*===========================================================================*/

static volatile uint32_t meas_count;
static volatile uint32_t meas_skip;
static volatile uint32_t meas_wraps;
static volatile uint32_t meas_t_start;
static volatile uint32_t meas_t_end;
static volatile bool meas_done;

static void meas_cb(PWMDriver *pwmp) {
  uint32_t now = time_us();
  uint32_t n = meas_count++;

  (void)pwmp;

  if (n == meas_skip) {
    meas_t_start = now;
  }
  else if (n == (meas_skip + meas_wraps)) {
    meas_t_end = now;
    meas_done = true;
  }
}

/*
 * Times @p wraps PWM wrap callbacks, skipping the first @p skip ones,
 * and returns the elapsed time in microseconds (0 on timeout).
 */
static uint32_t measure_wraps(PWMDriver *pwmp, uint32_t skip, uint32_t wraps,
                              uint32_t timeout_us) {
  uint32_t t0;

  chSysLock();
  meas_count   = 0U;
  meas_skip    = skip;
  meas_wraps   = wraps;
  meas_done    = false;
  chSysUnlock();

  pwmEnablePeriodicNotification(pwmp);

  t0 = time_us();
  while (!meas_done) {
    if ((uint32_t)(time_us() - t0) > timeout_us) {
      pwmDisablePeriodicNotification(pwmp);
      return 0U;
    }
  }

  pwmDisablePeriodicNotification(pwmp);

  return meas_t_end - meas_t_start;
}

static PWMConfig pwmcfg_meas = {
  .frequency = PWM_FREQ,
  .period    = PWM_PERIOD,
  .callback  = meas_cb,
  .channels  = {
    {.mode = PWM_OUTPUT_ACTIVE_HIGH, .callback = NULL},
    {.mode = PWM_OUTPUT_DISABLED,    .callback = NULL}
  }
};

/*===========================================================================*/
/* Stop safety machinery (PWMD1 + PWMD2).                                    */
/*===========================================================================*/

static volatile uint32_t stop_wraps;
static volatile bool stop_cb_fired;

static void stop_cb(PWMDriver *pwmp) {

  (void)pwmp;
  stop_wraps++;
  stop_cb_fired = true;
}

static PWMConfig pwmcfg_stop = {
  .frequency = PWM_FREQ,
  .period    = PWM_PERIOD,
  .callback  = stop_cb,
  .channels  = {
    {.mode = PWM_OUTPUT_DISABLED, .callback = NULL},
    {.mode = PWM_OUTPUT_DISABLED, .callback = NULL}
  }
};

static volatile uint32_t cross_wraps;

static void cross_cb(PWMDriver *pwmp) {

  (void)pwmp;
  cross_wraps++;
}

static PWMConfig pwmcfg_cross = {
  .frequency = PWM_FREQ,
  .period    = PWM_PERIOD,
  .callback  = cross_cb,
  .channels  = {
    {.mode = PWM_OUTPUT_DISABLED, .callback = NULL},
    {.mode = PWM_OUTPUT_DISABLED, .callback = NULL}
  }
};

/*===========================================================================*/
/* Individual tests.                                                         */
/*===========================================================================*/

/*
 * Test: 1000 wraps at 1kHz must take 1000000us within 0.05%.
 * The pre-fix driver programmed TOP=period making every cycle one
 * count too long (~1001000us here).
 */
static bool test_period_accuracy(void) {
  uint32_t elapsed = measure_wraps(&PWMD1, 2U, 1000U, 3000000U);

  chprintf(chp, "    1000 wraps of period %u: %u us (expected 1000000)\r\n",
           PWM_PERIOD, elapsed);

  return (elapsed >= 999500U) && (elapsed <= 1000500U);
}

/*
 * Test: width 500 of period 1000 samples as ~50% high.
 */
static bool test_duty_sanity(void) {
  uint32_t high = 0U;
  uint32_t total = 0U;
  uint32_t ratio;
  uint32_t t0;

  pwmEnableChannel(&PWMD1, 0U, 500U);

  /* Letting the new compare value take effect. */
  delay_us(3000U);

  t0 = time_us();
  while ((uint32_t)(time_us() - t0) < 50000U) {
    if (palReadLine(PWM_PIN) == PAL_HIGH) {
      high++;
    }
    total++;
  }

  ratio = (high * 100U) / total;
  chprintf(chp, "    %u/%u samples high (%u%%)\r\n", high, total, ratio);

  return (ratio >= 45U) && (ratio <= 55U);
}

/*
 * Test: after pwmChangePeriod() to 500 ticks, 1000 wraps must take
 * 500000us within 0.1%.
 */
static bool test_change_period(void) {
  uint32_t elapsed;

  pwmChangePeriod(&PWMD1, 500U);

  /* Skipping a few wraps so a counter above the new TOP can run out. */
  elapsed = measure_wraps(&PWMD1, 5U, 1000U, 3000000U);

  chprintf(chp, "    1000 wraps of period 500: %u us (expected 500000)\r\n",
           elapsed);

  return (elapsed >= 499500U) && (elapsed <= 500500U);
}

/*
 * Test: 500 start/stop cycles with the stop point swept across the
 * period. After each stop the callback must stay silent for 2ms.
 */
static bool test_stop_safety(void) {
  unsigned i;

  for (i = 0U; i < 500U; i++) {
    uint32_t t0;

    stop_wraps = 0U;
    pwmStart(&PWMD1, &pwmcfg_stop);
    pwmEnablePeriodicNotification(&PWMD1);

    /* Waiting for 3 wraps to prove the slice is alive. */
    t0 = time_us();
    while (stop_wraps < 3U) {
      if ((uint32_t)(time_us() - t0) > 20000U) {
        chprintf(chp, "    Cycle %u: slice did not wrap\r\n", i);
        pwmStop(&PWMD1);
        return false;
      }
    }

    /* Sweeping the stop point across the 1000us period. */
    delay_us((i * 29U) % 1000U);

    /* Keeping the wrap boundary away from the clear-to-stop window: a
       wrap landing between the flag clear and the stop's lock would
       deliver a legitimate pre-stop callback and read as a false
       failure. The sweep above still exercises every stop phase
       outside this guard band. */
    t0 = time_us();
    while (PWMD1.pwm->CH[PWMD1.timer_id].CTR > (PWM_PERIOD - 50U)) {
      if ((uint32_t)(time_us() - t0) > 5000U) {
        break;
      }
    }
    stop_cb_fired = false;

    pwmStop(&PWMD1);

    /* The flag was cleared before the stop and the wrap boundary is at
       least 50us away, so any callback recorded from here on is a
       stale post-stop delivery. */
    delay_us(2000U);
    if (stop_cb_fired) {
      chprintf(chp, "    Cycle %u: callback fired during or after pwmStop()\r\n", i);
      return false;
    }
  }

  return true;
}

/*
 * Test: the cross-check slice kept via the shared vector must still
 * be counting after all the start/stop cycles on the other slice.
 */
static bool test_cross_slice_alive(uint32_t before) {
  uint32_t mark;

  chprintf(chp, "    Cross slice wraps: %u before, %u after stop test\r\n",
           before, cross_wraps);

  if (cross_wraps <= (before + 1000U)) {
    return false;
  }

  /* Still counting right now. */
  mark = cross_wraps;
  delay_us(20000U);

  return cross_wraps >= (mark + 10U);
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
  bool ok;
  uint32_t cross_before;

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
  chprintf(chp, "  RP2040 PWM Validation\r\n");
  chprintf(chp, "========================================\r\n");
  chprintf(chp, "  Core clock: %u Hz\r\n",
           (unsigned)halClockGetPointX(RP_CLK_SYS));
  chprintf(chp, "  PWM pin:    GPIO%u (slice 1 channel A)\r\n", PWM_PIN);
  chprintf(chp, "\r\n");

  /* PWM output on GPIO2, pad input buffer stays enabled so the pin
     can be read back through SIO GPIO_IN. */
  palSetLineMode(PWM_PIN, PAL_MODE_ALTERNATE_PWM);

  /* Test 1: period accuracy. */
  pwmStart(&PWMD1, &pwmcfg_meas);
  ok = test_period_accuracy();
  report("Period accuracy (1000 wraps in 1000000us)", ok);

  /* Test 2: duty cycle sanity. */
  ok = test_duty_sanity();
  report("Duty sanity (width 500/1000 reads ~50%)", ok);

  /* Test 3: change_period while running. */
  ok = test_change_period();
  report("pwmChangePeriod (1000 wraps in 500000us)", ok);

  pwmStop(&PWMD1);

  /* Test 4: stop safety with a second slice as cross-check. */
  chprintf(chp, "\r\n  500 start/stop cycles...\r\n");
  cross_wraps = 0U;
  pwmStart(&PWMD2, &pwmcfg_cross);
  pwmEnablePeriodicNotification(&PWMD2);
  cross_before = cross_wraps;

  ok = test_stop_safety();
  report("No callback after pwmStop (500 cycles)", ok);

  /* Test 5: second slice unaffected by the other slice stopping. */
  ok = test_cross_slice_alive(cross_before);
  report("Other slice callbacks survive pwmStop", ok);

  pwmStop(&PWMD2);

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
    chThdSleepMilliseconds(1000);
  }

  return 0;
}
