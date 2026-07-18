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
 * RP2350 I2C abort-path validation test.
 *
 * Validates what an empty bus permits, no external wiring is required:
 * I2C0 runs on GPIO4 (SDA) / GPIO5 (SCL) with the internal pad pull-ups
 * enabled, no slave is attached.  Every transfer addressed to the absent
 * device 0x50 must be NACKed on the address byte, the driver must abort
 * quickly (well before the operation timeout), report MSG_RESET with
 * i2cGetErrors() == I2C_ACK_FAILURE and be immediately reusable with the
 * state machine back in I2C_READY.
 *
 * This exercises:
 *  - the NACK abort source to I2C_ACK_FAILURE error mapping,
 *  - the abort path of the write, write-then-read (repeated START) and
 *    pure read transfer shapes,
 *  - driver robustness across repeated aborts and i2cStop()/i2cStart()
 *    cycles.
 *
 * Single-core only.
 */

#include "ch.h"
#include "hal.h"
#include "chprintf.h"

#define LED_PIN              25U
#define UART_TX_PIN          0U
#define UART_RX_PIN          1U
#define I2C_SDA_PIN          4U
#define I2C_SCL_PIN          5U

/* Nothing lives at this address on the (empty) bus. */
#define ABSENT_ADDR          0x50U

/* Per-transfer operation timeout, the abort must happen much earlier. */
#define OP_TIMEOUT           TIME_MS2I(100)

/* An address NACK abort must complete within a few SCL periods, allow a
   generous margin that is still far below OP_TIMEOUT. */
#define ABORT_DEADLINE       TIME_MS2I(50)

static BaseSequentialStream *chp;
static unsigned pass_count;
static unsigned fail_count;

static const I2CConfig i2ccfg = {
  .baudrate = 100000U,
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
 * Runs one transfer against the absent address and checks the abort
 * behavior: MSG_RESET (not MSG_TIMEOUT), quick completion, exactly
 * I2C_ACK_FAILURE reported and driver back in I2C_READY.
 */
static bool check_nack_abort(size_t txbytes, size_t rxbytes) {
  static const uint8_t txbuf[2] = {0x00U, 0x00U};
  uint8_t rxbuf[4];
  systime_t start;
  sysinterval_t elapsed;
  i2cflags_t errors;
  msg_t msg;

  start = chVTGetSystemTime();
  if (txbytes > 0U) {
    msg = i2cMasterTransmitTimeout(&I2CD0, ABSENT_ADDR, txbuf, txbytes,
                                   rxbuf, rxbytes, OP_TIMEOUT);
  }
  else {
    msg = i2cMasterReceiveTimeout(&I2CD0, ABSENT_ADDR, rxbuf, rxbytes,
                                  OP_TIMEOUT);
  }
  elapsed = chVTTimeElapsedSinceX(start);
  errors = i2cGetErrors(&I2CD0);

  if (msg != MSG_RESET) {
    chprintf(chp, "    tx=%u rx=%u: expected MSG_RESET got %d\r\n",
             (unsigned)txbytes, (unsigned)rxbytes, (int)msg);
    return false;
  }

  if (elapsed >= ABORT_DEADLINE) {
    chprintf(chp, "    tx=%u rx=%u: abort took %u ticks\r\n",
             (unsigned)txbytes, (unsigned)rxbytes, (unsigned)elapsed);
    return false;
  }

  if (errors != I2C_ACK_FAILURE) {
    chprintf(chp, "    tx=%u rx=%u: expected errors 0x%02X got 0x%02X\r\n",
             (unsigned)txbytes, (unsigned)rxbytes,
             (unsigned)I2C_ACK_FAILURE, (unsigned)errors);
    return false;
  }

  if (I2CD0.state != I2C_READY) {
    chprintf(chp, "    tx=%u rx=%u: driver state %d, not I2C_READY\r\n",
             (unsigned)txbytes, (unsigned)rxbytes, (int)I2CD0.state);
    return false;
  }

  return true;
}

/*===========================================================================*/
/* Individual tests.                                                         */
/*===========================================================================*/

/*
 * Test: a single 2-byte write to the absent address is NACKed on the
 * address byte and reported as an ACK failure.
 */
static bool test_address_nack_write(void) {

  return check_nack_abort(2U, 0U);
}

/*
 * Test: repeated aborts do not accumulate state, the driver is reusable
 * on every iteration.
 */
static bool test_repeated_nack_write(void) {
  unsigned i;

  for (i = 0U; i < 10U; i++) {
    if (!check_nack_abort(2U, 0U)) {
      chprintf(chp, "    Failed at iteration %u\r\n", i);
      return false;
    }
  }

  return true;
}

/*
 * Test: a write-then-read request (repeated START shape) to the absent
 * address aborts just as cleanly.
 */
static bool test_nack_write_then_read(void) {

  return check_nack_abort(1U, 2U);
}

/*
 * Test: a pure read request to the absent address aborts cleanly.
 */
static bool test_nack_pure_read(void) {

  return check_nack_abort(0U, 2U);
}

/*
 * Test: driver survives a full stop/start cycle after aborts.
 */
static bool test_stop_start_cycle(void) {

  i2cStop(&I2CD0);
  i2cStart(&I2CD0, &i2ccfg);

  return check_nack_abort(2U, 0U);
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

  halInit();
  chSysInit();

  /* LED. */
  palSetLineMode(LED_PIN, PAL_MODE_OUTPUT_PUSHPULL | PAL_RP_PAD_DRIVE12);

  /* UART on GPIO0/GPIO1. */
  palSetLineMode(UART_TX_PIN, PAL_MODE_ALTERNATE_UART);
  palSetLineMode(UART_RX_PIN, PAL_MODE_ALTERNATE_UART);
  sioStart(&SIOD0, NULL);
  chp = (BaseSequentialStream *)&SIOD0;

  /* I2C0 on GPIO4 (SDA) / GPIO5 (SCL) with internal pull-ups, no
     external devices are attached. */
  palSetLineMode(I2C_SDA_PIN, PAL_MODE_ALTERNATE_I2C | PAL_RP_PAD_PUE);
  palSetLineMode(I2C_SCL_PIN, PAL_MODE_ALTERNATE_I2C | PAL_RP_PAD_PUE);
  i2cStart(&I2CD0, &i2ccfg);

  /* Start blinker. */
  chThdCreateStatic(waThread1, sizeof(waThread1), NORMALPRIO, Thread1, NULL);

  /* Small delay to let UART settle and pull-ups charge the bus. */
  chThdSleepMilliseconds(100);

  chprintf(chp, "\r\n");
  chprintf(chp, "========================================\r\n");
  chprintf(chp, "  RP2350 I2C Abort-Path Validation\r\n");
  chprintf(chp, "========================================\r\n");
  chprintf(chp, "  I2C0 @ 100kHz, SDA=GP%u SCL=GP%u\r\n",
           I2C_SDA_PIN, I2C_SCL_PIN);
  chprintf(chp, "  Target address: 0x%02X (absent)\r\n", ABSENT_ADDR);
  chprintf(chp, "\r\n");

  /* Test 1: single address-NACK write. */
  ok = test_address_nack_write();
  report("Address NACK write reports ACK failure", ok);

  /* Test 2: repeated aborts. */
  ok = test_repeated_nack_write();
  report("10 repeated NACK aborts, driver reusable", ok);

  /* Test 3: write-then-read shape. */
  ok = test_nack_write_then_read();
  report("Write-then-read request aborts cleanly", ok);

  /* Test 4: pure read shape. */
  ok = test_nack_pure_read();
  report("Pure read request aborts cleanly", ok);

  /* Test 5: stop/start cycle. */
  ok = test_stop_start_cycle();
  report("NACK abort after i2cStop/i2cStart cycle", ok);

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
