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

#include "ch.h"
#include "hal.h"
#include "hazard3_irq.h"

#define VALIDATION_LOW_IRQ                  46U
#define VALIDATION_HIGH_IRQ                 47U
#define VALIDATION_SWITCH_IRQ               48U
#define VALIDATION_MEICONTEXT_PRIORITY_MASK  0xFF1F0000U
#define VALIDATION_ROLLOVER_HI              0x12345678U
#define VALIDATION_MTIME_CTRL               \
  (*(volatile uint32_t *)(RISCV_SIO_BASE + RISCV_SIO_MTIME_CTRL_OFFSET))

#define VALIDATION_STR2(x)                  #x
#define VALIDATION_STR(x)                   VALIDATION_STR2(x)

semaphore_t smp_sem;
volatile bool smp_run;
volatile bool smp_done;
volatile uint32_t smp_count;

static semaphore_t ext_switch_sem;
static semaphore_t ext_switch_done_sem;
static volatile uint8_t irq_sequence[3];
static volatile uint8_t irq_sequence_count;
static volatile bool breakpoint_expected;
static volatile uint32_t breakpoint_status;
static volatile bool ext_switch_context_ok;
static volatile bool ext_switch_handoff_direct;
static volatile bool ext_switch_irq_entered;
static volatile bool ext_switch_main_resumed;
static volatile bool ext_switch_timer_fired;
static volatile bool ext_switch_timer_observed;
static volatile uint32_t pmp_probe_pc;
static volatile uint32_t pmp_probe_status;
static bool timer_rollover_passed;
static unsigned failures;

void validation_pmp_store(volatile uint32_t *addr);
void validation_breakpoint(void);
bool validation_thread_breakpoint_probe(void);
bool validation_context_switch_probe(void);

__attribute__((noinline, used))
void validation_context_switch_window(void) {
  unsigned i;

  for (i = 0U; i < 32U; i++) {
    chThdSleepMicroseconds(100U);
  }
}

void _exception_handler(uint32_t mcause, uint32_t mtval,
                        struct port_extctx *frame) {

  (void)mtval;

  if (breakpoint_expected && (mcause == 3U)) {
    breakpoint_expected = false;
    breakpoint_status = 1U;
    frame->mepc += 4U;
    return;
  }

  if ((pmp_probe_status == 0U) &&
      (pmp_probe_pc != 0U) &&
      (mcause == 7U) &&
      (frame->mepc == pmp_probe_pc)) {
    pmp_probe_status = 1U;
    frame->mepc += 4U;
    return;
  }

  pmp_probe_status = 2U;
  __asm__ volatile ("csrci mstatus, 0x8");
  while (true) {
    __asm__ volatile ("wfi");
  }
}

static void write_text(const char *s) {
  size_t n;

  for (n = 0U; s[n] != '\0'; n++) {
  }
  chnWrite(&SIOD0, (const uint8_t *)s, n);
}

static void report(const char *name, bool passed) {

  if (passed) {
    write_text("PASS: ");
  }
  else {
    write_text("FAIL: ");
    failures++;
  }
  write_text(name);
  write_text("\r\n");
}

static void test_timer_rollover_preinit(void) {
  uint32_t mip;
  unsigned i;
  bool programmed;

  VALIDATION_MTIME_CTRL = 0U;
  MTIMECMP_LO = UINT32_MAX;
  MTIMECMP_HI = UINT32_MAX;
  MTIME_HI = VALIDATION_ROLLOVER_HI;
  MTIME_LO = 0xFFFFFFF0U;

  /* Exercise the port's 32-bit alarm extension while MTIME is stopped. The
     requested low word is after the imminent rollover, so the compare high
     word must be advanced exactly once.*/
  port_timer_set_alarm((systime_t)0x20U);
  programmed = (MTIMECMP_HI == (VALIDATION_ROLLOVER_HI + 1U)) &&
               (MTIMECMP_LO == 0x20U);

  VALIDATION_MTIME_CTRL = MTIME_CTRL_EN;

  /* MTIP is observable even though mie.MTIE and global MIE are still clear.*/
  for (i = 0U; i < 1000000U; i++) {
    __asm__ volatile ("csrr %0, mip" : "=r"(mip));
    if ((mip & MIP_MTIP) != 0U) {
      break;
    }
  }

  timer_rollover_passed = programmed && (i < 1000000U) &&
                          (MTIME_HI == (VALIDATION_ROLLOVER_HI + 1U));
  port_timer_stop_alarm();
}

static void timer_callback(virtual_timer_t *vtp, void *arg) {
  semaphore_t *sp;

  sp = (semaphore_t *)arg;
  chSysLockFromISR();
  chSemSignalI(sp);
  chSysUnlockFromISR();
  (void)vtp;
}

static void test_isa(void) {
  uint32_t misa;
  uint32_t required;

  __asm__ volatile ("csrr %0, misa" : "=r"(misa));
  required = (1U << 0) | (1U << 2) | (1U << 8) | (1U << 12);
  report("RV32 I/M/A/C ISA", (misa & required) == required);
}

static void test_context_switch_abi(void) {

  report("callee-saved registers across context switches",
         validation_context_switch_probe());
}

static void test_thread_exception_restore(void) {
  bool registers_ok;

  breakpoint_expected = true;
  breakpoint_status = 0U;
  registers_ok = validation_thread_breakpoint_probe();
  if (breakpoint_expected) {
    breakpoint_status = 2U;
    breakpoint_expected = false;
  }

  report("thread exception caller-register restoration",
         registers_ok && (breakpoint_status == 1U));
}

static void test_pmp_guard(void) {
  uint32_t pmpaddr;
  uint32_t pmpcfg;
  uint32_t pmpcfgm;
  uint32_t cfg;
  bool passed;

  __asm__ volatile ("csrr %0, " VALIDATION_STR(PORT_GUARD_PMPADDR_CSR)
                    : "=r"(pmpaddr));
  __asm__ volatile ("csrr %0, " VALIDATION_STR(PORT_GUARD_PMPCFG_CSR)
                    : "=r"(pmpcfg));
  __asm__ volatile ("csrr %0, " VALIDATION_STR(CSR_PMPCFGM0)
                    : "=r"(pmpcfgm));

  cfg = (pmpcfg >> PORT_GUARD_PMPCFG_SHIFT) & 0xFFU;
  passed = (pmpaddr != 0U) &&
           (cfg == PMP_CFG_A_NAPOT) &&
           ((pmpcfgm & (1U << PORT_USE_GUARD_PMP_REGION)) != 0U);
  report("PMP stack guard configuration", passed);
}

static void test_timer_reprogramming(void) {
  virtual_timer_t vt;
  semaphore_t sem;
  msg_t msg;
  unsigned i;
  bool passed;

  chVTObjectInit(&vt);
  chSemObjectInit(&sem, (cnt_t)0);
  passed = true;

  for (i = 0U; i < 200U; i++) {
    chVTSet(&vt, TIME_US2I(200U), timer_callback, &sem);
    chVTSet(&vt, TIME_US2I(40U + (i & 31U)), timer_callback, &sem);
    msg = chSemWaitTimeout(&sem, TIME_MS2I(10U));
    if (msg != MSG_OK) {
      passed = false;
      break;
    }
  }

  chVTReset(&vt);
  report("tickless alarm reprogramming stress", passed);
}

static void ext_switch_timer_callback(virtual_timer_t *vtp, void *arg) {

  ext_switch_timer_fired = true;
  (void)vtp;
  (void)arg;
}

static THD_WORKING_AREA(wa_ext_switch, 512);
static THD_FUNCTION(ext_switch_thread, arg) {
  uint32_t meicontext;
  uint32_t mie;
  uint32_t start;

  (void)arg;
  chSemWait(&ext_switch_sem);

  /* This thread is deliberately awakened by an external IRQ. It runs before
     the interrupted lower-priority thread can resume its trap continuation. */
  ext_switch_handoff_direct = ext_switch_irq_entered &&
                              !ext_switch_main_resumed;
  __asm__ volatile ("csrr %0, " VALIDATION_STR(CSR_MEICONTEXT)
                    : "=r"(meicontext));
  __asm__ volatile ("csrr %0, mie" : "=r"(mie));
  ext_switch_context_ok =
      ((meicontext & (MEICONTEXT_NOIRQ | MEICONTEXT_MRETEIRQ)) ==
       MEICONTEXT_NOIRQ) &&
      ((meicontext & VALIDATION_MEICONTEXT_PRIORITY_MASK) == 0U) &&
      ((mie & MIE_MTIE) != 0U);

  /* Keep this higher-priority thread runnable while waiting. The old thread
     therefore cannot finish the suspended external-IRQ epilogue; the virtual
     timer can fire only if that epilogue completed Xh3irq return before the
     scheduler switch. MTIME itself keeps running even if MTIE is masked, so
     the loop has a deterministic failure bound.*/
  start = MTIME_LO;
  while (!ext_switch_timer_fired &&
         ((uint32_t)(MTIME_LO - start) < 20000U)) {
  }
  ext_switch_timer_observed = ext_switch_timer_fired;
  chSemSignal(&ext_switch_done_sem);
}

OSAL_IRQ_HANDLER(Vector100) {

  OSAL_IRQ_PROLOGUE();

  nvicClearPending(VALIDATION_SWITCH_IRQ);
  ext_switch_irq_entered = true;
  chSysLockFromISR();
  chSemSignalI(&ext_switch_sem);
  chSysUnlockFromISR();

  OSAL_IRQ_EPILOGUE();
}

static void test_external_irq_preemption(void) {
  virtual_timer_t vt;
  msg_t msg;
  bool passed;

  chSemObjectInit(&ext_switch_sem, (cnt_t)0);
  chSemObjectInit(&ext_switch_done_sem, (cnt_t)0);
  chVTObjectInit(&vt);
  ext_switch_context_ok = false;
  ext_switch_handoff_direct = false;
  ext_switch_irq_entered = false;
  ext_switch_main_resumed = false;
  ext_switch_timer_fired = false;
  ext_switch_timer_observed = false;

  (void)chThdCreateStatic(wa_ext_switch, sizeof(wa_ext_switch),
                          NORMALPRIO + 1U, ext_switch_thread, NULL);
  chVTSet(&vt, TIME_US2I(2000U), ext_switch_timer_callback, NULL);
  nvicEnableVector(VALIDATION_SWITCH_IRQ, 1U);
  nvicSetPending(VALIDATION_SWITCH_IRQ);

  /* If ISR-exit preemption works, the higher-priority waiter runs before this
     loop can complete. Waiting for the ISR marker avoids relying on the exact
     instruction at which the core recognizes a newly forced interrupt. */
  while (!ext_switch_irq_entered) {
  }
  ext_switch_main_resumed = true;

  msg = chSemWaitTimeout(&ext_switch_done_sem, TIME_MS2I(50U));
  nvicDisableVector(VALIDATION_SWITCH_IRQ);
  chVTReset(&vt);

  passed = (msg == MSG_OK) && ext_switch_handoff_direct &&
           ext_switch_context_ok &&
           ext_switch_timer_observed;
  report("external IRQ preemption context handoff", passed);
}

OSAL_IRQ_HANDLER(VectorF8) {
  uint32_t i;

  OSAL_IRQ_PROLOGUE();

  nvicClearPending(VALIDATION_LOW_IRQ);
  irq_sequence[irq_sequence_count] = 1U;
  irq_sequence_count++;
  nvicSetPending(VALIDATION_HIGH_IRQ);

  for (i = 0U; (i < 100000U) && (irq_sequence_count < 2U); i++) {
    __asm__ volatile ("nop");
  }

  if (irq_sequence_count < 3U) {
    irq_sequence[irq_sequence_count] = 3U;
    irq_sequence_count++;
  }

  OSAL_IRQ_EPILOGUE();
}

OSAL_IRQ_HANDLER(VectorFC) {

  OSAL_IRQ_PROLOGUE();

  nvicClearPending(VALIDATION_HIGH_IRQ);
  breakpoint_expected = true;
  validation_breakpoint();
  if (breakpoint_expected) {
    breakpoint_status = 2U;
    breakpoint_expected = false;
  }
  if (irq_sequence_count < 3U) {
    irq_sequence[irq_sequence_count] = 2U;
    irq_sequence_count++;
  }

  OSAL_IRQ_EPILOGUE();
}

static void test_irq_nesting(void) {
  unsigned i;
  bool passed;

  irq_sequence_count = 0U;
  irq_sequence[0] = 0U;
  irq_sequence[1] = 0U;
  irq_sequence[2] = 0U;
  breakpoint_expected = false;
  breakpoint_status = 0U;

  nvicEnableVector(VALIDATION_LOW_IRQ, 3U);
  nvicEnableVector(VALIDATION_HIGH_IRQ, 0U);
  nvicSetPending(VALIDATION_LOW_IRQ);

  for (i = 0U; (i < 100U) && (irq_sequence_count < 3U); i++) {
    chThdSleepMicroseconds(50U);
  }

  nvicDisableVector(VALIDATION_HIGH_IRQ);
  nvicDisableVector(VALIDATION_LOW_IRQ);

  passed = (irq_sequence_count == 3U) &&
           (irq_sequence[0] == 1U) &&
           (irq_sequence[1] == 2U) &&
           (irq_sequence[2] == 3U) &&
           (breakpoint_status == 1U);
  report("nested Xh3irq and exception restoration", passed);
}

static void test_smp(void) {
  msg_t msg;
  unsigned i;
  bool passed;

  __atomic_store_n(&smp_run, true, __ATOMIC_RELEASE);
  passed = true;

  for (i = 0U; i < 64U; i++) {
    msg = chSemWaitTimeout(&smp_sem, TIME_MS2I(20U));
    if (msg != MSG_OK) {
      passed = false;
      break;
    }
  }

  passed = passed && __atomic_load_n(&smp_done, __ATOMIC_ACQUIRE) &&
           (__atomic_load_n(&smp_count, __ATOMIC_RELAXED) == 64U);
  report("SMP cross-core semaphore and core-local timer", passed);
}

static void test_pmp_store_fault(void) {
  volatile uint32_t *guardp;

  guardp = (volatile uint32_t *)(void *)chThdGetSelfX()->wabase;
  pmp_probe_pc = (uint32_t)(uintptr_t)validation_pmp_store;
  pmp_probe_status = 0U;
  validation_pmp_store(guardp);
  pmp_probe_pc = 0U;
  report("PMP guarded-stack store fault", pmp_probe_status == 1U);
}

int main(void) {
  bool led_state;

  chSemObjectInit(&smp_sem, (cnt_t)0);
  smp_run = false;
  smp_done = false;
  smp_count = 0U;
  failures = 0U;
  test_timer_rollover_preinit();

  halInit();
  chSysInit();

  palSetLineMode(0U, PAL_MODE_ALTERNATE_UART);
  palSetLineMode(1U, PAL_MODE_ALTERNATE_UART);
  palSetLineMode(25U, PAL_MODE_OUTPUT_PUSHPULL | PAL_RP_PAD_DRIVE12);
  sioStart(&SIOD0, NULL);

  write_text("Hazard3/RP2350 validation\r\n");
  test_isa();
  test_context_switch_abi();
  test_thread_exception_restore();
  test_pmp_guard();
  report("tickless alarm across MTIME low-word rollover",
         timer_rollover_passed);
  test_timer_reprogramming();
  test_irq_nesting();
  test_external_irq_preemption();
  test_smp();
  test_pmp_store_fault();

  if (failures == 0U) {
    write_text("RESULT: PASS\r\n");
  }
  else {
    write_text("RESULT: FAIL\r\n");
  }

  led_state = false;
  while (true) {
    led_state = !led_state;
    palWriteLine(25U, led_state ? PAL_HIGH : PAL_LOW);
    chThdSleepMilliseconds(failures == 0U ? 500U : 100U);
  }
}
