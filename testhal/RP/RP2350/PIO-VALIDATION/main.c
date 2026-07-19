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
 * RP PIOv1 driver validation.
 *
 * Exercises the PIO state machine and instruction memory allocator:
 *
 * 1. JMP relocation: a relocatable square-wave program loaded at a
 *    non-zero offset must have its program-relative JMP target adjusted
 *    by the load offset. A "jmp 0" parking program pinned at address 0
 *    makes the failure deterministic: without relocation the state
 *    machine jumps to absolute address 0 and sticks there, the pin
 *    never toggles.
 * 2. Instruction memory allocation masks for a full 32-instruction
 *    program (1U << 32 is undefined behavior, evaluating to 1 on ARM,
 *    so an unfixed driver tracks a full-memory program with an empty
 *    mask and lets further loads overlap it).
 * 3. Block reset lifetime: loaded programs must survive freeing the
 *    last state machine and loading must work before the first state
 *    machine allocation (unfixed, the block is still held in reset and
 *    the instruction memory writes are lost).
 * 4. Cross-core free: a state machine allocated by core 0 and freed by
 *    core 1 must actually be released so that all four state machines
 *    of the block can be allocated again.
 *
 * The square wave is emitted on GPIO2 and read back through SIO GPIO_IN.
 * The report is emitted on UART0 (GPIO0/GPIO1) at the SIO default
 * configuration bitrate (115200-8-N-1, SIO_DEFAULT_BITRATE override in
 * this project's halconf.h).
 */

#include "ch.h"
#include "hal.h"
#include "chprintf.h"

#include "pio_validation.h"

/*===========================================================================*/
/* Shared state, plain SRAM is coherent between the RP2350 cores.            */
/*===========================================================================*/

volatile uint32_t c1_ready;
volatile uint32_t c1_do_free;
volatile uint32_t c1_free_done;
const rp_pio_sm_t * volatile xcore_smp;

/*===========================================================================*/
/* Test parameters.                                                          */
/*===========================================================================*/

#define TEST_GPIO           2U
#define TEST_IRQ_PRIORITY   3U

/* State machine clock and measurement window.*/
#define SM_TEST_FREQ        10000U      /* SM clock in Hz.                  */
#define MEASURE_US          100000U     /* 100 ms sampling window.          */

/* The square-wave program loops over 3 instructions producing 2 edges per
   loop, expected edge count in the 100 ms window with a +/-30% margin.*/
#define EXPECTED_EDGES      ((SM_TEST_FREQ / 10U) * 2U / 3U)
#define EDGES_LO            ((EXPECTED_EDGES * 7U) / 10U)
#define EDGES_HI            ((EXPECTED_EDGES * 13U) / 10U)

/*===========================================================================*/
/* PIO programs.                                                             */
/*===========================================================================*/

/*
 * Relocatable square-wave program:
 *   .wrap_target                       (not used, loops via jmp)
 *     set pins, 1     ; 0xE001  111_00000_000_00001 (dest pins=000, data=1)
 *     set pins, 0     ; 0xE000
 *     jmp 0           ; 0x0000  000_00000_000_00000 (cond always, addr 0,
 *                                program-relative, needs relocation)
 */
static const uint16_t sqwave_instructions[] = {
  0xE001U,
  0xE000U,
  0x0000U
};

static const rp_pio_program_t sqwave_program = {
  .instructions = sqwave_instructions,
  .length       = 3U,
  .origin       = -1
};

/*
 * Parking program pinned at address 0: "jmp 0" spins forever at address 0,
 * so an unrelocated JMP landing there sticks and the test pin stops
 * toggling, which makes the discrimination deterministic.
 */
static const uint16_t park_instructions[] = {
  0x0000U
};

static const rp_pio_program_t park_program = {
  .instructions = park_instructions,
  .length       = 1U,
  .origin       = 0
};

/*
 * 32 x nop (mov y, y = 0xA042: 101_00000_010_00_010) filling the whole
 * instruction memory.
 */
static uint16_t nop32_instructions[RP_PIO_NUM_INSTR_MEM];

static const rp_pio_program_t nop32_program = {
  .instructions = nop32_instructions,
  .length       = RP_PIO_NUM_INSTR_MEM,
  .origin       = -1
};

/* Single nop program.*/
static const uint16_t single_instructions[] = {
  0xA042U
};

static const rp_pio_program_t single_program = {
  .instructions = single_instructions,
  .length       = 1U,
  .origin       = -1
};

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
/* Measurement helpers.                                                      */
/*===========================================================================*/

static void delay_us(uint32_t us) {
  uint32_t start = TIMER0->TIMERAWL;

  while ((uint32_t)(TIMER0->TIMERAWL - start) < us) {
  }
}

/**
 * @brief   Counts edges on TEST_GPIO by sampling SIO GPIO_IN for 100 ms.
 */
static uint32_t count_edges(void) {
  uint32_t edges = 0U;
  uint32_t start = TIMER0->TIMERAWL;
  uint32_t prev = (SIO->GPIO_IN >> TEST_GPIO) & 1U;

  while ((uint32_t)(TIMER0->TIMERAWL - start) < MEASURE_US) {
    uint32_t cur = (SIO->GPIO_IN >> TEST_GPIO) & 1U;

    if (cur != prev) {
      edges++;
      prev = cur;
    }
  }

  return edges;
}

/**
 * @brief   Samples the state machine PC and checks it stays in a window.
 */
static bool check_addr_window(const rp_pio_sm_t *smp,
                              uint32_t lo, uint32_t hi) {
  unsigned i;

  for (i = 0U; i < 64U; i++) {
    uint32_t addr = pioSmGetAddrX(smp);

    if ((addr < lo) || (addr > hi)) {
      return false;
    }
    delay_us(37U);
  }

  return true;
}

/*===========================================================================*/
/* State machine setup.                                                      */
/*===========================================================================*/

/**
 * @brief   Configures a state machine for the square-wave program and
 *          starts it from @p offset.
 */
static void sqwave_start(const rp_pio_sm_t *smp, uint32_t offset) {

  pioSmDisableX(smp);

  /* SM clock, clkdiv computed from the system clock.*/
  pioSmSetFrequencyX(smp, SM_TEST_FREQ);

  /* Full-range wrap, the program loops via its own JMP.*/
  pioSmSetExecctrlX(smp, PIO_SM_EXECCTRL_WRAP(0U, 31U));

  /* Default shift directions.*/
  pioSmSetShiftctrlX(smp, PIO_SM_SHIFTCTRL_IN_SHIFTDIR |
                          PIO_SM_SHIFTCTRL_OUT_SHIFTDIR);

  /* SET pin group: one pin based at TEST_GPIO.*/
  pioSmSetPinctrlX(smp, (1U << PIO_SM_PINCTRL_SET_COUNT_Pos) |
                        (TEST_GPIO << PIO_SM_PINCTRL_SET_BASE_Pos));

  /* Route the pad to the owning PIO block.*/
  pioSmSetPinFunctionX(smp, TEST_GPIO);

  /* Clean restart.*/
  pioSmClearFifosX(smp);
  pioClearDebugX(smp);
  pioSmRestartX(smp);
  pioSmClkdivRestartX(smp);

  /* Pin direction to output: "set pindirs, 1" through the SET group.*/
  pioSmExecX(smp, 0xE081U);

  /* Jump to the program start and go.*/
  pioSmSetPCX(smp, offset);
  pioSmEnableX(smp);
}

/*===========================================================================*/
/* Application entry point, core 0.                                          */
/*===========================================================================*/

int main(void) {
  const rp_pio_block_t *block = RP_PIO0_BLOCK;
  const rp_pio_sm_t *smp;
  const rp_pio_sm_t *sms[RP_PIO_NUM_STATE_MACHINES];
  int32_t park_off, sq_off, off32, off1;
  uint32_t edges;
  unsigned i;
  bool ok;

  halInit();
  chSysInit();

  /* UART0 console on GPIO0/GPIO1, halconf sets SIO default to 115200.*/
  palSetLineMode(0U, PAL_MODE_ALTERNATE_UART);
  palSetLineMode(1U, PAL_MODE_ALTERNATE_UART);
  sioStart(&SIOD0, NULL);

  palSetLineMode(25U, PAL_MODE_OUTPUT_PUSHPULL);

  /* Test pin: PIO0 function with pad input enable so that the generated
     square wave can be read back through SIO GPIO_IN.*/
  palSetLineMode(TEST_GPIO, PAL_MODE_ALTERNATE_PIO0);

  chprintf(chp, "\r\n*** PIO validation\r\n");
  chprintf(chp, "*** Expected edges per window: %u (%u..%u)\r\n",
           EXPECTED_EDGES, EDGES_LO, EDGES_HI);

  /* Waiting for core 1 to come alive.*/
  while (c1_ready == 0U) {
    chThdSleepMilliseconds(1);
  }

  /*
   * Test 1: JMP relocation on program load.
   */
  chprintf(chp, "--- Test 1: JMP relocation\r\n");

  smp = pioSmAlloc(block, 0U, TEST_IRQ_PRIORITY, NULL, NULL);
  report("SM0 allocated", smp != NULL);

  park_off = pioProgramLoad(block, &park_program);
  report("parking program pinned at 0", park_off == 0);

  sq_off = pioProgramLoad(block, &sqwave_program);
  report("square wave loaded at nonzero offset", sq_off >= 1);

  /* Dependent steps only run with valid prerequisites, a failed
     allocation or load must not be dereferenced or used as an
     offset.*/
  if ((smp != NULL) && (park_off == 0) && (sq_off >= 1)) {
    sqwave_start(smp, (uint32_t)sq_off);
    edges = count_edges();
    chprintf(chp, "      edges: %u\r\n", edges);
    report("edge count in window", (edges >= EDGES_LO) && (edges <= EDGES_HI));
    report("PC stays within program",
           check_addr_window(smp, (uint32_t)sq_off, (uint32_t)sq_off + 2U));

    pioSmDisableX(smp);
  }
  else {
    report("edge count in window", false);
    report("PC stays within program", false);
    goto summary;
  }

  /*
   * Test 2: full instruction memory allocation masks.
   *
   * Note: on Cortex-M the pre-fix undefined expression (1U << 32)
   * happens to evaluate through a register LSL to the correct mask, so
   * this leg regression-tests the behavior but cannot discriminate the
   * undefined-behavior fix on this target; that is a compile-time
   * property covered by UBSan/static analysis, not by this run.
   */
  chprintf(chp, "--- Test 2: 32-instruction masks\r\n");

  pioProgramUnload(block, sq_off, sqwave_program.length);
  pioProgramUnload(block, park_off, park_program.length);

  for (i = 0U; i < RP_PIO_NUM_INSTR_MEM; i++) {
    nop32_instructions[i] = 0xA042U;    /* NOP encoded as mov y, y.*/
  }

  off32 = pioProgramLoad(block, &nop32_program);
  report("32-instruction program loads at 0", off32 == 0);

  off1 = pioProgramLoad(block, &single_program);
  report("full memory rejects further load", off1 == -1);

  pioProgramUnload(block, off32, nop32_program.length);

  off1 = pioProgramLoad(block, &single_program);
  report("reload after unload succeeds", off1 >= 0);

  pioProgramUnload(block, off1, single_program.length);

  /*
   * Test 3: block reset lifetime vs. instruction memory.
   */
  chprintf(chp, "--- Test 3: reset lifetime\r\n");

  /* 3a: instruction memory must survive freeing the last state machine.*/
  sq_off = pioProgramLoad(block, &sqwave_program);
  report("square wave reloaded", sq_off >= 0);

  sqwave_start(smp, (uint32_t)sq_off);
  edges = count_edges();
  chprintf(chp, "      edges before free: %u\r\n", edges);
  report("running before free", (edges >= EDGES_LO) && (edges <= EDGES_HI));

  pioSmFree(smp);                       /* Last SM of the block.*/
  smp = pioSmAlloc(block, 0U, TEST_IRQ_PRIORITY, NULL, NULL);
  report("SM0 re-allocated", smp != NULL);

  /* Restart without reloading the program.*/
  sqwave_start(smp, (uint32_t)sq_off);
  edges = count_edges();
  chprintf(chp, "      edges after realloc: %u\r\n", edges);
  report("imem survives last SM free",
         (edges >= EDGES_LO) && (edges <= EDGES_HI));

  pioSmDisableX(smp);

  /* 3b: loading must work before any state machine is allocated.*/
  pioProgramUnload(block, sq_off, sqwave_program.length);
  pioSmFree(smp);                       /* Block fully idle, gets reset.*/

  sq_off = pioProgramLoad(block, &sqwave_program);
  report("load before first alloc accepted", sq_off >= 0);

  smp = pioSmAlloc(block, 0U, TEST_IRQ_PRIORITY, NULL, NULL);
  report("SM0 allocated after load", smp != NULL);

  sqwave_start(smp, (uint32_t)sq_off);
  edges = count_edges();
  chprintf(chp, "      edges after early load: %u\r\n", edges);
  report("load before first alloc works",
         (edges >= EDGES_LO) && (edges <= EDGES_HI));

  pioSmDisableX(smp);
  pioProgramUnload(block, sq_off, sqwave_program.length);

  /*
   * Test 4: cross-core free, core 1 frees a state machine allocated by
   * core 0, afterwards all four state machines must be allocatable.
   */
  chprintf(chp, "--- Test 4: cross-core free\r\n");

  xcore_smp = smp;                      /* SM0, allocated by core 0.*/
  __DMB();
  c1_do_free = 1U;

  for (i = 0U; (c1_free_done == 0U) && (i < 1000U); i++) {
    chThdSleepMilliseconds(1);
  }
  report("core 1 free completed", c1_free_done != 0U);

  ok = true;
  for (i = 0U; i < RP_PIO_NUM_STATE_MACHINES; i++) {
    sms[i] = pioSmAlloc(block, RP_PIO_SM_ID_ANY, TEST_IRQ_PRIORITY,
                        NULL, NULL);
    if (sms[i] == NULL) {
      ok = false;
    }
  }
  report("all four SMs allocatable after cross-core free", ok);

  for (i = 0U; i < RP_PIO_NUM_STATE_MACHINES; i++) {
    if (sms[i] != NULL) {
      pioSmFree(sms[i]);
    }
  }

  /*
   * Summary.
   */
summary:
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
