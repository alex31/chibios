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

#include <zephyr/kernel.h>
#include <zephyr/irq.h>
#include <zephyr/irq_offload.h>
#include <zephyr/version.h>

/*
 * Zephyr sandbox guest demo. A self-test exercises the port at startup,
 * the LED is on while the test runs, then it blinks slowly on success or
 * fast on failure. The result is printed on the standard output and kept
 * in the "test_result" structure, to be inspected with a debugger.
 * The LED is expected as VIO GPIO unit 0, if the host does not assign it
 * then the LED operations have no effect.
 * Fatal errors terminate the sandbox with the CHIBIOS_SB_EXIT_HALT exit
 * code ORed with the reason.
 */

/*===========================================================================*/
/* Demo settings.                                                            */
/*===========================================================================*/

#define TEST_DURATION_MS        2000U
#define PERIODIC_PERIOD_MS      10U
#define TIMER_PERIOD_MS         50U
#define IDLE_DURATION_MS        100U
#define CATCHUP_DURATION_MS     100U
#define SWVRQ_CYCLES            100U
#define OFFLOAD_CYCLES          10U
#define THREADS_STACK_SIZE      1024

#define BLINK_PASS_PERIOD_MS    500
#define BLINK_FAIL_PERIOD_MS    100

/* VIO GPIO unit of the LED.*/
#define LED_VGPIO_UNIT          0U

/* Software-triggered VRQ.*/
#define SWVRQ                   30U

/* Threads priorities, lower values are higher priorities. The test runs
   in the main thread.*/
#define SWVRQ_PRIO              1
#define TEST_PRIO               2
#define PERIODIC_PRIO           3
#define CONSUMER_PRIO           4
#define PRODUCER_PRIO           5

/* Failure bits.*/
#define FAIL_PERIODIC           (1U << 0)
#define FAIL_TIMER              (1U << 1)
#define FAIL_QUEUE              (1U << 2)
#define FAIL_TICK_DRIFT         (1U << 3)
#define FAIL_SWVRQ              (1U << 4)
#define FAIL_OFFLOAD            (1U << 5)
#define FAIL_IDLE_TIME          (1U << 6)
#define FAIL_TICK_CATCHUP       (1U << 7)

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
  uint32_t      offload;
  uint32_t      host_frequency;
};

volatile struct test_result test_result;

static volatile uint32_t periodic_count;
static volatile uint32_t timer_count;
static volatile uint32_t consumer_count;
static volatile uint32_t sequence_errors;
static volatile uint32_t swvrq_count;
static volatile uint32_t offload_count;

K_MSGQ_DEFINE(queue, sizeof (uint32_t), 4, 4);
K_SEM_DEFINE(swvrq_sem, 0, 1);

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
static void swvrq_isr(const void *arg) {

  ARG_UNUSED(arg);

  k_sem_give(&swvrq_sem);
}

static void timer_expiry(struct k_timer *timer) {

  ARG_UNUSED(timer);

  timer_count++;
}

K_TIMER_DEFINE(test_timer, timer_expiry, NULL);

static void offload_routine(const void *arg) {
  volatile bool *in_isr = (volatile bool *)arg;

  *in_isr = k_is_in_isr();
}

/*
 * Periodic thread, preempts the producer on ticks.
 */
static void periodic_thread(void *p1, void *p2, void *p3) {
  int64_t next;

  ARG_UNUSED(p1);
  ARG_UNUSED(p2);
  ARG_UNUSED(p3);

  next = k_uptime_get();
  for (;;) {
    next += PERIODIC_PERIOD_MS;
    (void)k_sleep(K_TIMEOUT_ABS_MS(next));
    periodic_count++;
  }
}

/*
 * Producer thread, never blocks, each send wakes the higher priority
 * consumer.
 */
static void producer_thread(void *p1, void *p2, void *p3) {
  uint32_t seq = 0U;

  ARG_UNUSED(p1);
  ARG_UNUSED(p2);
  ARG_UNUSED(p3);

  for (;;) {
    (void)k_msgq_put(&queue, &seq, K_FOREVER);
    seq++;
  }
}

static void consumer_thread(void *p1, void *p2, void *p3) {
  uint32_t expected = 0U;
  uint32_t seq;

  ARG_UNUSED(p1);
  ARG_UNUSED(p2);
  ARG_UNUSED(p3);

  for (;;) {
    (void)k_msgq_get(&queue, &seq, K_FOREVER);
    if (seq != expected) {
      sequence_errors++;
    }
    expected = seq + 1U;
    consumer_count++;
  }
}

static void swvrq_thread(void *p1, void *p2, void *p3) {

  ARG_UNUSED(p1);
  ARG_UNUSED(p2);
  ARG_UNUSED(p3);

  for (;;) {
    (void)k_sem_take(&swvrq_sem, K_FOREVER);
    swvrq_count++;
  }
}

K_THREAD_DEFINE(swvrq_tid, THREADS_STACK_SIZE, swvrq_thread,
                NULL, NULL, NULL, SWVRQ_PRIO, 0, 0);
K_THREAD_DEFINE(periodic_tid, THREADS_STACK_SIZE, periodic_thread,
                NULL, NULL, NULL, PERIODIC_PRIO, 0, 0);
K_THREAD_DEFINE(consumer_tid, THREADS_STACK_SIZE, consumer_thread,
                NULL, NULL, NULL, CONSUMER_PRIO, 0, 0);
K_THREAD_DEFINE(producer_tid, THREADS_STACK_SIZE, producer_thread,
                NULL, NULL, NULL, PRODUCER_PRIO, 0, 0);

static uint32_t self_test(void) {
  uint32_t failures = 0U;
  uint32_t host_per_ms, periodic, timer, consumer, host_start, host_elapsed;
  int64_t start, elapsed;
  unsigned i;

  host_per_ms = chibios_sb_get_frequency() / 1000U;
  test_result.host_frequency = chibios_sb_get_frequency();

  /* Concurrent activity, the producer runs whenever the other threads
     are blocked.*/
  k_timer_start(&test_timer, K_MSEC(TIMER_PERIOD_MS),
                K_MSEC(TIMER_PERIOD_MS));
  periodic   = periodic_count;
  timer      = timer_count;
  consumer   = consumer_count;
  start      = k_uptime_get();
  host_start = chibios_sb_get_systime();
  k_msleep(TEST_DURATION_MS);
  host_elapsed = chibios_sb_get_systime() - host_start;
  elapsed      = k_uptime_get() - start;
  periodic     = periodic_count - periodic;
  timer        = timer_count - timer;
  consumer     = consumer_count - consumer;
  k_thread_suspend(producer_tid);

  test_result.periodic = periodic;
  test_result.timer    = timer;
  test_result.consumer = consumer;

  if (!in_range(periodic, TEST_DURATION_MS / PERIODIC_PERIOD_MS, 1U)) {
    failures |= FAIL_PERIODIC;
  }
  if (!in_range(timer, TEST_DURATION_MS / TIMER_PERIOD_MS, 1U)) {
    failures |= FAIL_TIMER;
  }
  if ((consumer == 0U) || (sequence_errors != 0U)) {
    failures |= FAIL_QUEUE;
  }
  if (!in_range(host_elapsed, (uint32_t)elapsed * host_per_ms,
                host_per_ms)) {
    failures |= FAIL_TICK_DRIFT;
  }

  /* Preemption from a VRQ handler, the swvrq thread must have run before
     returning from the VRQ trigger.*/
  IRQ_CONNECT(SWVRQ, 0, swvrq_isr, NULL, 0);
  irq_enable(SWVRQ);
  for (i = 0U; i < SWVRQ_CYCLES; i++) {
    uint32_t before = swvrq_count;

    (void)chibios_sb_vrq_setwt(BIT(SWVRQ));
    if (swvrq_count != before + 1U) {
      failures |= FAIL_SWVRQ;
    }
  }
  test_result.swvrq = swvrq_count;

  /* Offloaded routines must run synchronously in VRQ context.*/
  for (i = 0U; i < OFFLOAD_CYCLES; i++) {
    volatile bool in_isr = false;

    irq_offload(offload_routine, (const void *)&in_isr);
    if (in_isr) {
      offload_count++;
    }
    else {
      failures |= FAIL_OFFLOAD;
    }
  }
  test_result.offload = offload_count;

  /* Idle phase, the idle thread releases the CPU to the host.*/
  host_start = chibios_sb_get_systime();
  k_msleep(IDLE_DURATION_MS);
  host_elapsed = chibios_sb_get_systime() - host_start;
  if (!in_range(host_elapsed, IDLE_DURATION_MS * host_per_ms,
                host_per_ms)) {
    failures |= FAIL_IDLE_TIME;
  }

  /* Tick catch-up, the whole sandbox is blocked in the host, the elapsed
     time must be recovered on return.*/
  start = k_uptime_get();
  chibios_sb_sleep(CATCHUP_DURATION_MS * host_per_ms);
  elapsed = k_uptime_get() - start;
  if (!in_range((uint32_t)elapsed, CATCHUP_DURATION_MS, 1U)) {
    failures |= FAIL_TICK_CATCHUP;
  }

  /* Stopping the test activity.*/
  k_timer_stop(&test_timer);
  k_thread_suspend(periodic_tid);

  return failures;
}

/*===========================================================================*/
/* Application entry point.                                                  */
/*===========================================================================*/

int main(void) {
  uint32_t failures;
  int32_t period;

  k_thread_priority_set(k_current_get(), TEST_PRIO);

  printk("\nZephyr %s on ChibiOS sandbox, self-test running...\n",
         KERNEL_VERSION_STRING);
  chibios_sb_vgpio_write(LED_VGPIO_UNIT, 1U);

  failures = self_test();
  test_result.failures = failures;
  test_result.state    = 1U;

  printk("periodic=%u timer=%u consumer=%u swvrq=%u offload=%u "
         "result=0x%08x %s\n",
         test_result.periodic, test_result.timer, test_result.consumer,
         test_result.swvrq, test_result.offload, failures,
         (failures == 0U) ? "PASSED" : "FAILED");

  period = (failures == 0U) ? BLINK_PASS_PERIOD_MS : BLINK_FAIL_PERIOD_MS;
  for (;;) {
    k_msleep(period);
    chibios_sb_vgpio_toggle(LED_VGPIO_UNIT, 1U);
  }

  return 0;
}
