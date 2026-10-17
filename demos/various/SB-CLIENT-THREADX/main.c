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

#include <stdio.h>
#include <string.h>

#include "tx_api.h"

#include "sbuservio.h"

/*
 * ThreadX sandbox guest demo. A self-test exercises the port at startup,
 * the LED is on while the test runs, then it blinks slowly on success or
 * fast on failure. The result is printed on the standard output and kept
 * in the "test_result" structure, to be inspected with a debugger.
 * The LED is expected as VIO GPIO unit 0, if the host does not assign it
 * then the LED operations have no effect.
 * A thread stack overflow terminates the sandbox with exit code
 * EXIT_STACK_OVERFLOW.
 * In FPU builds the FPU registers are also checked, they must be preserved
 * by voluntary context switches and by VRQ preemptions.
 */

/*===========================================================================*/
/* Demo settings.                                                            */
/*===========================================================================*/

#define TICKS_PER_MS            (TX_TIMER_TICKS_PER_SECOND / 1000UL)
#define TEST_DURATION           (2000UL * TICKS_PER_MS)
#define PERIODIC_PERIOD         (10UL * TICKS_PER_MS)
#define TIMER_PERIOD            (50UL * TICKS_PER_MS)
#define IDLE_DURATION           (100UL * TICKS_PER_MS)
#define CATCHUP_DURATION_MS     100U
#define SWVRQ_CYCLES            100U
#define STACK_SIZE              1024U
#define QUEUE_LENGTH            4U
#define FPU_SPIN_DURATION_MS    200U
#define FPU_SPIN_LOOPS          20000U

#define BLINK_PASS_PERIOD       (500UL * TICKS_PER_MS)
#define BLINK_FAIL_PERIOD       (100UL * TICKS_PER_MS)

/* Sandbox exit code of a thread stack overflow.*/
#define EXIT_STACK_OVERFLOW     0xA6000000U

/* VIO GPIO unit of the LED.*/
#define LED_VGPIO_UNIT          0U

/* Software-triggered VRQ, no suffix because it is pasted into the
   handler name by TX_SB_VRQ_HANDLER().*/
#define SWVRQ                   31

/* Threads priorities, lower values are higher priorities.*/
#define SWVRQ_PRIO              1U
#define TEST_PRIO               2U
#define PERIODIC_PRIO           3U
#define BLINKER_PRIO            3U
#define CONSUMER_PRIO           4U
#define PRODUCER_PRIO           5U
#define FPU_PRIO                1U

/* Failure bits.*/
#define FAIL_PERIODIC           (1U << 0)
#define FAIL_TIMER              (1U << 1)
#define FAIL_QUEUE              (1U << 2)
#define FAIL_TICK_DRIFT         (1U << 3)
#define FAIL_SWVRQ              (1U << 4)
#define FAIL_IDLE_TIME          (1U << 5)
#define FAIL_TICK_CATCHUP       (1U << 6)
#define FAIL_CREATE             (1U << 7)
#define FAIL_FPU                (1U << 8)

/*===========================================================================*/
/* Demo variables.                                                           */
/*===========================================================================*/

/**
 * @brief   Self-test result, the state is 1 when the test is complete.
 */
struct test_result {
  uint32_t      state;
  uint32_t      failures;
  uint32_t      periodic;
  uint32_t      timer;
  uint32_t      consumer;
  uint32_t      swvrq;
  uint32_t      host_frequency;
  uint32_t      fpu_checks;
  uint32_t      fpu_switches;
};

volatile struct test_result test_result;

static TX_THREAD swvrq_thread, test_thread, periodic_thread;
static TX_THREAD blinker_thread, consumer_thread, producer_thread;
static ULONG swvrq_stack[STACK_SIZE / sizeof (ULONG)];
static ULONG test_stack[STACK_SIZE / sizeof (ULONG)];
static ULONG periodic_stack[STACK_SIZE / sizeof (ULONG)];
static ULONG blinker_stack[STACK_SIZE / sizeof (ULONG)];
static ULONG consumer_stack[STACK_SIZE / sizeof (ULONG)];
static ULONG producer_stack[STACK_SIZE / sizeof (ULONG)];

static TX_QUEUE queue;
static ULONG queue_storage[QUEUE_LENGTH];
static TX_SEMAPHORE swvrq_sem;
static TX_TIMER timer;

static char result_string[128];

static volatile uint32_t periodic_count;
static volatile uint32_t timer_count;
static volatile uint32_t consumer_count;
static volatile uint32_t sequence_errors;
static volatile uint32_t swvrq_count;

/*===========================================================================*/
/* FPU check functions.                                                      */
/*===========================================================================*/

#if defined(__ARM_FP)
/*
 * FPU registers pattern, the FPSCR value selects a rounding mode.
 */
struct fpu_pattern {
  uint32_t      s[32];
  uint32_t      fpscr;
};

static struct fpu_pattern fpu_pattern_test, fpu_pattern_a, fpu_pattern_b;
static TX_THREAD fpu_a_thread, fpu_b_thread;
static ULONG fpu_a_stack[STACK_SIZE / sizeof (ULONG)];
static ULONG fpu_b_stack[STACK_SIZE / sizeof (ULONG)];
static volatile uint32_t fpu_errors;

#define FPU_CLOBBERS                                                        \
  "s0", "s1", "s2", "s3", "s4", "s5", "s6", "s7",                           \
  "s8", "s9", "s10", "s11", "s12", "s13", "s14", "s15",                     \
  "s16", "s17", "s18", "s19", "s20", "s21", "s22", "s23",                   \
  "s24", "s25", "s26", "s27", "s28", "s29", "s30", "s31"

static void fpu_pattern_init(struct fpu_pattern *p, uint32_t id,
                             uint32_t fpscr) {
  unsigned i;

  for (i = 0U; i < 32U; i++) {
    p->s[i] = 0xA5000000U | (id << 16) | i;
  }
  p->fpscr = fpscr;
}

/*
 * Loads a pattern in all the FPU registers and spins, the thread is
 * preempted by VRQs meanwhile, then checks the registers.
 */
static bool fpu_spin_check(const struct fpu_pattern *p, uint32_t loops) {
  uint32_t s[32];
  uint32_t fpscr;

  __asm__ volatile ("vmsr     FPSCR, %[fpscr_in]          \n\t"
                    "vldmia   %[src], {s0-s31}            \n\t"
                    "1:                                   \n\t"
                    "subs     %[n], %[n], #1              \n\t"
                    "bne      1b                          \n\t"
                    "vstmia   %[dst], {s0-s31}            \n\t"
                    "vmrs     %[fpscr_out], FPSCR"
                    : [n] "+r" (loops), [fpscr_out] "=r" (fpscr)
                    : [src] "r" (p->s), [dst] "r" (s),
                      [fpscr_in] "r" (p->fpscr)
                    : FPU_CLOBBERS, "cc", "memory");

  return (memcmp(s, p->s, sizeof (s)) == 0) && (fpscr == p->fpscr);
}

/*
 * FPU thread, loads its pattern in all the FPU registers then sleeps, the
 * callee-saved registers and FPSCR must be preserved by the switches.
 */
static VOID fpu_entry(ULONG input) {
  const struct fpu_pattern *p = (const struct fpu_pattern *)input;
  uint32_t s[16];
  uint32_t fpscr;

  for (;;) {
    __asm__ volatile ("vmsr     FPSCR, %[fpscr_in]        \n\t"
                      "vldmia   %[src], {s0-s31}"
                      :
                      : [src] "r" (p->s), [fpscr_in] "r" (p->fpscr)
                      : FPU_CLOBBERS, "memory");
    (void)tx_thread_sleep(1U);
    __asm__ volatile ("vstmia   %[dst], {s16-s31}         \n\t"
                      "vmrs     %[fpscr_out], FPSCR"
                      : [fpscr_out] "=r" (fpscr)
                      : [dst] "r" (s)
                      : "memory");
    if ((memcmp(s, &p->s[16], sizeof (s)) != 0) || (fpscr != p->fpscr)) {
      fpu_errors++;
    }
    test_result.fpu_switches++;
  }
}

/*
 * FPU check, the FPU threads load their patterns on each tick while the
 * test thread spins with its own pattern.
 */
static uint32_t fpu_check(void) {
  systime_t start;
  sysinterval_t duration;
  uint32_t failures = 0U;

  (void)tx_thread_resume(&fpu_a_thread);
  (void)tx_thread_resume(&fpu_b_thread);

  duration = (sbGetFrequency() / 1000U) * FPU_SPIN_DURATION_MS;
  start    = sbGetSystemTime();
  while ((sysinterval_t)(sbGetSystemTime() - start) < duration) {
    if (!fpu_spin_check(&fpu_pattern_test, FPU_SPIN_LOOPS)) {
      failures |= FAIL_FPU;
    }
    test_result.fpu_checks++;
  }

  (void)tx_thread_suspend(&fpu_a_thread);
  (void)tx_thread_suspend(&fpu_b_thread);

  if ((fpu_errors != 0U) || (test_result.fpu_switches == 0U)) {
    failures |= FAIL_FPU;
  }

  return failures;
}
#endif /* defined(__ARM_FP) */

/*===========================================================================*/
/* Self-test functions.                                                      */
/*===========================================================================*/

static bool in_range(uint32_t value, uint32_t expected, uint32_t tolerance) {

  return (value + tolerance >= expected) &&
         (value <= expected + tolerance);
}

/*
 * VRQ handler, wakes the swvrq thread which has higher priority than the
 * thread triggering the VRQ.
 */
TX_SB_VRQ_HANDLER(SWVRQ) {

  (void)tx_semaphore_put(&swvrq_sem);
}

static VOID stack_error(TX_THREAD *thread_ptr) {

  (void)thread_ptr;

  sbExit((msg_t)EXIT_STACK_OVERFLOW);
}

static VOID timer_expiry(ULONG input) {

  (void)input;

  timer_count++;
}

/*
 * LED blinker thread, the blink period shows the self-test result.
 */
static VOID blinker_entry(ULONG input) {
  ULONG failures;

  (void)input;

  vgpioWrite(LED_VGPIO_UNIT, 1U);
  (void)tx_thread_suspend(tx_thread_identify());
  failures = test_result.failures;

  for (;;) {
    (void)tx_thread_sleep((failures == 0U) ? BLINK_PASS_PERIOD :
                                             BLINK_FAIL_PERIOD);
    vgpioToggle(LED_VGPIO_UNIT, 1U);
  }
}

/*
 * Periodic thread, preempts the producer on ticks.
 */
static VOID periodic_entry(ULONG input) {
  ULONG next;

  (void)input;

  next = tx_time_get();
  for (;;) {
    next += PERIODIC_PERIOD;
    (void)tx_thread_sleep(next - tx_time_get());
    periodic_count++;
  }
}

/*
 * Producer thread, never blocks, each send wakes the higher priority
 * consumer.
 */
static VOID producer_entry(ULONG input) {
  ULONG seq = 0U;

  (void)input;

  for (;;) {
    (void)tx_queue_send(&queue, &seq, TX_WAIT_FOREVER);
    seq++;
  }
}

static VOID consumer_entry(ULONG input) {
  ULONG expected = 0U;
  ULONG seq;

  (void)input;

  for (;;) {
    (void)tx_queue_receive(&queue, &seq, TX_WAIT_FOREVER);
    if (seq != expected) {
      sequence_errors++;
    }
    expected = seq + 1U;
    consumer_count++;
  }
}

static VOID swvrq_entry(ULONG input) {

  (void)input;

  for (;;) {
    (void)tx_semaphore_get(&swvrq_sem, TX_WAIT_FOREVER);
    swvrq_count++;
  }
}

static VOID test_entry(ULONG input) {
  uint32_t failures = 0U;
  uint32_t host_per_tick, periodic, timer_n, consumer;
  systime_t host_start, host_elapsed;
  ULONG start, ticks;
  unsigned i;

  (void)input;

  test_result.host_frequency = sbGetFrequency();
  host_per_tick = sbGetFrequency() / (uint32_t)TX_TIMER_TICKS_PER_SECOND;

  /* Concurrent activity, the producer runs whenever the other threads
     are blocked.*/
  periodic   = periodic_count;
  timer_n    = timer_count;
  consumer   = consumer_count;
  start      = tx_time_get();
  host_start = sbGetSystemTime();
  (void)tx_thread_sleep(TEST_DURATION);
  host_elapsed = sbGetSystemTime() - host_start;
  ticks        = tx_time_get() - start;
  periodic     = periodic_count - periodic;
  timer_n      = timer_count - timer_n;
  consumer     = consumer_count - consumer;
  (void)tx_thread_suspend(&producer_thread);

  test_result.periodic = periodic;
  test_result.timer    = timer_n;
  test_result.consumer = consumer;

  if (!in_range(periodic, TEST_DURATION / PERIODIC_PERIOD, 1U)) {
    failures |= FAIL_PERIODIC;
  }
  if (!in_range(timer_n, TEST_DURATION / TIMER_PERIOD, 1U)) {
    failures |= FAIL_TIMER;
  }
  if ((consumer == 0U) || (sequence_errors != 0U)) {
    failures |= FAIL_QUEUE;
  }
  if (!in_range(host_elapsed, ticks * host_per_tick, host_per_tick)) {
    failures |= FAIL_TICK_DRIFT;
  }

  /* Preemption from a VRQ handler, the swvrq thread must have run before
     returning from the VRQ trigger.*/
  tx_sb_vrq_enable(SWVRQ);
  for (i = 0U; i < SWVRQ_CYCLES; i++) {
    uint32_t before = swvrq_count;

    tx_sb_vrq_trigger(SWVRQ);
    if (swvrq_count != before + 1U) {
      failures |= FAIL_SWVRQ;
    }
  }
  test_result.swvrq = swvrq_count;

#if defined(__ARM_FP)
  /* FPU registers preservation.*/
  failures |= fpu_check();
#endif

  /* Idle phase, the scheduler releases the CPU to the host.*/
  host_start = sbGetSystemTime();
  (void)tx_thread_sleep(IDLE_DURATION);
  host_elapsed = sbGetSystemTime() - host_start;
  if (!in_range(host_elapsed, IDLE_DURATION * host_per_tick,
                host_per_tick)) {
    failures |= FAIL_IDLE_TIME;
  }

  /* Tick catch-up, the whole sandbox is blocked in the host while the
     alarm VRQs are merged, the elapsed ticks must be recovered when the
     pending VRQ is served on return.*/
  start = tx_time_get();
  sbSleepMilliseconds(CATCHUP_DURATION_MS);
  ticks = tx_time_get() - start;
  if (!in_range(ticks, CATCHUP_DURATION_MS * TICKS_PER_MS, 1U)) {
    failures |= FAIL_TICK_CATCHUP;
  }

  /* Stopping the test activity.*/
  (void)tx_timer_deactivate(&timer);
  (void)tx_thread_suspend(&periodic_thread);

  failures |= test_result.failures;
  test_result.failures = failures;
  test_result.state    = 1U;

  (void)snprintf(result_string, sizeof (result_string),
                 "ThreadX %d.%d.%d on ARMvx-M-SB: periodic=%lu timer=%lu "
                 "consumer=%lu swvrq=%lu result=0x%08lx",
                 THREADX_MAJOR_VERSION, THREADX_MINOR_VERSION,
                 THREADX_PATCH_VERSION, (unsigned long)periodic,
                 (unsigned long)timer_n, (unsigned long)consumer,
                 (unsigned long)swvrq_count, (unsigned long)failures);
  printf("%s\n", result_string);

  (void)tx_thread_resume(&blinker_thread);
}

/*===========================================================================*/
/* ThreadX initialization.                                                   */
/*===========================================================================*/

static void create_thread(TX_THREAD *thread_ptr, CHAR *name,
                          VOID (*entry)(ULONG), ULONG *stack, UINT prio) {

  if (tx_thread_create(thread_ptr, name, entry, 0U, stack, STACK_SIZE,
                       prio, prio, TX_NO_TIME_SLICE,
                       TX_AUTO_START) != TX_SUCCESS) {
    test_result.failures |= FAIL_CREATE;
  }
}

/*
 * Objects creation, the first unused memory is not used because all the
 * objects are static.
 */
VOID tx_application_define(VOID *first_unused_memory) {

  (void)first_unused_memory;

  (void)tx_thread_stack_error_notify(stack_error);

  if ((tx_queue_create(&queue, "queue", TX_1_ULONG, queue_storage,
                       sizeof (queue_storage)) != TX_SUCCESS) ||
      (tx_semaphore_create(&swvrq_sem, "swvrq", 0U) != TX_SUCCESS) ||
      (tx_timer_create(&timer, "timer", timer_expiry, 0U, TIMER_PERIOD,
                       TIMER_PERIOD, TX_AUTO_ACTIVATE) != TX_SUCCESS)) {
    test_result.failures |= FAIL_CREATE;
  }

  create_thread(&swvrq_thread, "swvrq", swvrq_entry, swvrq_stack,
                SWVRQ_PRIO);
  create_thread(&test_thread, "test", test_entry, test_stack, TEST_PRIO);
  create_thread(&periodic_thread, "periodic", periodic_entry,
                periodic_stack, PERIODIC_PRIO);
  create_thread(&blinker_thread, "blinker", blinker_entry, blinker_stack,
                BLINKER_PRIO);
  create_thread(&consumer_thread, "consumer", consumer_entry,
                consumer_stack, CONSUMER_PRIO);
  create_thread(&producer_thread, "producer", producer_entry,
                producer_stack, PRODUCER_PRIO);

#if defined(__ARM_FP)
  /* The FPU threads are started by the FPU check.*/
  fpu_pattern_init(&fpu_pattern_test, 0U, 0x00C00000U);
  fpu_pattern_init(&fpu_pattern_a, 1U, 0x00400000U);
  fpu_pattern_init(&fpu_pattern_b, 2U, 0x00800000U);
  if ((tx_thread_create(&fpu_a_thread, "fpu_a", fpu_entry,
                        (ULONG)&fpu_pattern_a, fpu_a_stack, STACK_SIZE,
                        FPU_PRIO, FPU_PRIO, TX_NO_TIME_SLICE,
                        TX_DONT_START) != TX_SUCCESS) ||
      (tx_thread_create(&fpu_b_thread, "fpu_b", fpu_entry,
                        (ULONG)&fpu_pattern_b, fpu_b_stack, STACK_SIZE,
                        FPU_PRIO, FPU_PRIO, TX_NO_TIME_SLICE,
                        TX_DONT_START) != TX_SUCCESS)) {
    test_result.failures |= FAIL_CREATE;
  }
#endif
}

/*===========================================================================*/
/* Application entry point.                                                  */
/*===========================================================================*/

int main(void) {

  /* It never returns, the sandbox runs until terminated by the host.*/
  tx_kernel_enter();

  return 0;
}
