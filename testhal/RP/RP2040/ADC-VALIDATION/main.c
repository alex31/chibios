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
 * RP2040 ADC start-failure validation test.
 *
 * Validates that adcStart() correctly reports DMA channel allocation
 * failure instead of leaving the driver falsely in the ADC_READY
 * state:
 *
 *   Test A: all DMA channels are exhausted before adcStart(); the
 *           call must fail (not HAL_RET_SUCCESS) and the driver must
 *           remain in ADC_STOP.
 *   Test B: the DMA channels are released and adcStart() is retried;
 *           it must succeed and a synchronous conversion of the
 *           internal temperature sensor must return a plausible
 *           sample.
 *
 * Output is on SIOD0 (UART0, GPIO0/GPIO1, 38400 8N1 default).
 * Single-core only.
 */

#include "ch.h"
#include "hal.h"
#include "rp_dma.h"
#include "chprintf.h"

#define LED_PIN              25U
#define UART_TX_PIN          0U
#define UART_RX_PIN          1U

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

/*===========================================================================*/
/* DMA channel hogging.                                                      */
/*===========================================================================*/

static const rp_dma_channel_t *hogged[RP_DMA_NUM_CHANNELS];
static unsigned hogged_count;

/*
 * Allocates every free DMA channel so that a subsequent adcStart()
 * cannot obtain one.  Returns the number of channels taken.
 */
static unsigned dma_hog_all(void) {

  hogged_count = 0U;
  while (hogged_count < RP_DMA_NUM_CHANNELS) {
    const rp_dma_channel_t *dmachp;

    dmachp = dmaChannelAlloc(RP_DMA_CHANNEL_ID_ANY, 3U, NULL, NULL);
    if (dmachp == NULL) {
      break;
    }
    hogged[hogged_count++] = dmachp;
  }

  return hogged_count;
}

/*
 * Releases all channels taken by dma_hog_all().
 */
static void dma_release_all(void) {

  while (hogged_count > 0U) {
    hogged_count--;
    dmaChannelFree(hogged[hogged_count]);
  }
}

/*===========================================================================*/
/* ADC conversion group.                                                     */
/*===========================================================================*/

/*
 * Single sample of the internal temperature sensor, linear buffer,
 * free-running conversion clock, temperature sensor enabled.
 */
static const ADCConversionGroup tempgrp = {
  .circular     = false,
  .num_channels = 1U,
  .end_cb       = NULL,
  .error_cb     = NULL,
  .channel      = ADC_CHANNEL_TEMPSENSOR,
  .rrobin       = 0U,
  .div          = 0U,
  .ts_enabled   = true
};

static adcsample_t samples[1];

/*===========================================================================*/
/* Individual tests.                                                        */
/*===========================================================================*/

/*
 * Test A: adcStart() with all DMA channels exhausted must fail and
 * leave the driver in ADC_STOP.
 */
static void test_start_failure(void) {
  unsigned taken;
  msg_t msg;

  taken = dma_hog_all();
  chprintf(chp, "  DMA channels hogged: %u of %u\r\n",
           taken, (unsigned)RP_DMA_NUM_CHANNELS);
  report("DMA hog acquired channels (exhaustion proven by next check)",
         taken > 0U);

  msg = adcStart(&ADCD1, NULL);
  chprintf(chp, "  adcStart() returned %d, state %d\r\n",
           (int)msg, (int)ADCD1.state);
  report("adcStart fails without free DMA channel",
         msg != HAL_RET_SUCCESS);
  report("Driver state remains ADC_STOP on failure",
         ADCD1.state == ADC_STOP);

  /* If the start unexpectedly succeeded (regression), restore ADC_STOP
     so the recovery test still starts from a known state. Under the
     historical regression the state lies (READY without a DMA channel)
     and adcStop() would free a NULL channel and halt; the driver is
     only stopped through the API when it actually holds one.*/
  if (ADCD1.state != ADC_STOP) {
    if (ADCD1.dma != NULL) {
      adcStop(&ADCD1);
    }
    else {
      /* Re-initializing through the public API instead of patching
         internal fields.*/
      adcObjectInit(&ADCD1);
    }
  }
}

/*
 * Test B: after releasing the DMA channels adcStart() must succeed
 * and a synchronous temperature sensor conversion must work.
 */
static void test_start_recovery(void) {
  msg_t msg;

  dma_release_all();
  report("Hogged DMA channels released", hogged_count == 0U);

  msg = adcStart(&ADCD1, NULL);
  chprintf(chp, "  adcStart() returned %d, state %d\r\n",
           (int)msg, (int)ADCD1.state);
  report("adcStart succeeds with DMA available",
         msg == HAL_RET_SUCCESS);
  report("Driver state is ADC_READY after start",
         ADCD1.state == ADC_READY);
  if (msg != HAL_RET_SUCCESS) {
    return;
  }

  /* Warm-up conversion: the temperature sensor needs to settle after
     TS_EN is first set, the initial conversion can flag ERR. Result and
     status are intentionally ignored.*/
  (void) adcConvert(&ADCD1, &tempgrp, samples, 1U);
  chThdSleepMilliseconds(1);

  samples[0] = 0U;
  msg = adcConvert(&ADCD1, &tempgrp, samples, 1U);
  chprintf(chp, "  adcConvert() returned %d, sample 0x%03X\r\n",
           (int)msg, (unsigned)samples[0]);
  report("Temperature sensor conversion completes", msg == MSG_OK);
  report("Temperature sensor sample is plausible",
         (samples[0] != 0U) && (samples[0] < 4096U));

  adcStop(&ADCD1);
  report("Driver stopped after conversion", ADCD1.state == ADC_STOP);
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
  chprintf(chp, "  RP2040 ADC Start-Failure Validation\r\n");
  chprintf(chp, "========================================\r\n");
  chprintf(chp, "\r\n");

  chprintf(chp, "  Test A: start with DMA exhausted...\r\n");
  test_start_failure();

  chprintf(chp, "\r\n  Test B: recovery after DMA release...\r\n");
  test_start_recovery();

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
