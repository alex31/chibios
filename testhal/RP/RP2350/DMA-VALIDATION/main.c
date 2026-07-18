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
 * RP DMAv1 validation.
 *
 * Exercises the DMA sharing helper on the RP2350:
 * - Memory-to-memory transfer through the channel API, including the
 *   RP2350 TRANS_COUNT MODE field protection of dmaChannelSetCounterX().
 * - Cross-core channel free: a channel allocated by core 0 is freed by
 *   core 1, then core 0 must be able to allocate all the channels again.
 *   Without the recorded-owner fix the freed channel remains marked as
 *   allocated by core 0 forever and the full re-allocation caps at 15.
 *
 * Report is emitted on UART0 (GPIO0/GPIO1) at the SIO default bitrate.
 */

#include "ch.h"
#include "hal.h"
#include "chprintf.h"

#include "dma_validation.h"

/*===========================================================================*/
/* Shared state, plain SRAM is coherent between the RP2350 cores.            */
/*===========================================================================*/

volatile uint32_t c1_ready;
volatile uint32_t c1_heartbeat;
volatile uint32_t c1_free_go;
volatile uint32_t c1_free_done;
const rp_dma_channel_t *volatile c1_free_chp;

/*===========================================================================*/
/* Report helpers.                                                           */
/*===========================================================================*/

static BaseSequentialStream *chp = (BaseSequentialStream *)&SIOD0;

static unsigned pass_count;
static unsigned fail_count;

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
/* Test 1: memory-to-memory transfer and TRANS_COUNT protection.             */
/*===========================================================================*/

#define M2M_WORDS           16U

static uint32_t m2m_src[M2M_WORDS];
static uint32_t m2m_dst[M2M_WORDS];

static void test_mem_to_mem(void) {
  const rp_dma_channel_t *dmachp;
  uint32_t tc;
  unsigned i;
  bool ok;

  chprintf(chp, "--- Test 1: mem-to-mem transfer\r\n");

  dmachp = dmaChannelAlloc(RP_DMA_CHANNEL_ID_ANY, 3U, NULL, NULL);
  report("channel allocated", dmachp != NULL);
  if (dmachp == NULL) {
    return;
  }

  for (i = 0U; i < M2M_WORDS; i++) {
    m2m_src[i] = 0xA5A50000U + i;
    m2m_dst[i] = 0U;
  }

  dmaChannelSetSourceX(dmachp, (uint32_t)m2m_src);
  dmaChannelSetDestinationX(dmachp, (uint32_t)m2m_dst);
  dmaChannelSetCounterX(dmachp, M2M_WORDS);

  /* Item 25 regression check: on the RP2350 the TRANS_COUNT bits [31:28]
     are the count MODE field, dmaChannelSetCounterX() must leave it at
     NORMAL (0) and preserve the 28-bit count.

     NOTE: dmaChannelSetCounterX() also carries a debug assertion catching
     counts that spill into the MODE field. Executing an out-of-range
     count here would halt the system in debug builds, so only the masking
     path is verified: after dmaChannelSetCounterX(16) the register must
     read back with count 16 and the mode nibble at 0.*/
  tc = dmachp->channel->TRANS_COUNT;
  report("TRANS_COUNT mode nibble is NORMAL", (tc >> 28) == 0U);
  report("TRANS_COUNT count preserved", (tc & 0x0FFFFFFFU) == M2M_WORDS);

  /* Unpaced word copy with incrementing addresses on both sides, then
     trigger by setting EN.*/
  dmaChannelSetModeX(dmachp, DMA_CTRL_TRIG_TREQ_PERMANENT |
                             DMA_CTRL_TRIG_DATA_SIZE_WORD |
                             DMA_CTRL_TRIG_INCR_READ |
                             DMA_CTRL_TRIG_INCR_WRITE);
  dmaChannelEnableX(dmachp);

  for (i = 0U; (i < 1000000U) && dmaChannelIsBusyX(dmachp); i++) {
  }
  report("transfer completed", dmaChannelIsBusyX(dmachp) == false);

  ok = true;
  for (i = 0U; i < M2M_WORDS; i++) {
    if (m2m_dst[i] != m2m_src[i]) {
      ok = false;
      break;
    }
  }
  report("data copied correctly", ok);

  dmaChannelFree(dmachp);
}

/*===========================================================================*/
/* Test 2: cross-core free, channel must return to the free pool.            */
/*===========================================================================*/

static const rp_dma_channel_t *all_channels[RP_DMA_NUM_CHANNELS];

static void test_cross_core_free(void) {
  const rp_dma_channel_t *dmachp;
  unsigned i, allocated;

  chprintf(chp, "--- Test 2: cross-core free\r\n");

  /* Core 0 allocates a specific channel...*/
  dmachp = dmaChannelAlloc(0U, 3U, NULL, NULL);
  report("channel 0 allocated on core 0", dmachp != NULL);
  if (dmachp == NULL) {
    return;
  }

  /* ...and core 1 frees it.*/
  c1_free_chp = dmachp;
  c1_free_go = 1U;
  for (i = 0U; (c1_free_done == 0U) && (i < 500U); i++) {
    chThdSleepMilliseconds(10);
  }
  report("core 1 performed the free", c1_free_done != 0U);

  /* Core 0 must now be able to allocate every channel. With the free
     keyed on SIO->CPUID the cross-core free clears nothing and the
     leaked channel caps the count at 15.*/
  allocated = 0U;
  for (i = 0U; i < RP_DMA_NUM_CHANNELS; i++) {
    all_channels[i] = dmaChannelAlloc(RP_DMA_CHANNEL_ID_ANY, 3U, NULL, NULL);
    if (all_channels[i] != NULL) {
      allocated++;
    }
  }
  chprintf(chp, "  channels allocated after cross-core free: %u of %u\r\n",
           allocated, (unsigned)RP_DMA_NUM_CHANNELS);
  report("all channels allocatable after cross-core free",
         allocated == RP_DMA_NUM_CHANNELS);

  for (i = 0U; i < RP_DMA_NUM_CHANNELS; i++) {
    if (all_channels[i] != NULL) {
      dmaChannelFree(all_channels[i]);
    }
  }
}

/*
 * Application entry point, core 0.
 */
int main(void) {
  unsigned i;

  halInit();
  chSysInit();

  /* UART0 console on GPIO0/GPIO1.*/
  palSetLineMode(0U, PAL_MODE_ALTERNATE_UART);
  palSetLineMode(1U, PAL_MODE_ALTERNATE_UART);
  sioStart(&SIOD0, NULL);

  palSetLineMode(25U, PAL_MODE_OUTPUT_PUSHPULL);

  chprintf(chp, "\r\n*** RP DMAv1 validation\r\n");

  /* Waiting for core 1 to come alive.*/
  for (i = 0U; (c1_ready == 0U) && (i < 500U); i++) {
    chThdSleepMilliseconds(10);
  }
  report("core 1 started", c1_ready != 0U);

  test_mem_to_mem();
  test_cross_core_free();

  chprintf(chp, "\r\nResults: %u pass, %u fail\r\n", pass_count, fail_count);
  if (fail_count == 0U) {
    chprintf(chp, "ALL TESTS PASSED\r\n");
  }
  else {
    chprintf(chp, "*** FAILURES DETECTED ***\r\n");
  }

  while (true) {
    palToggleLine(25U);
    chThdSleepMilliseconds(500);
  }
}
