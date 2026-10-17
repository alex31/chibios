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

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "timers.h"

#include "sbuservio.h"
#include "vserial.h"

/*
 * FreeRTOS sandbox guest demo. A self-test exercises the port at startup,
 * the LED is on while the test runs, then it blinks slowly on success or
 * fast on failure. The result is also printed on the standard output.
 * The LED is expected as VIO GPIO unit 0, if the host does not assign it
 * then the LED operations have no effect.
 * If the host assigns VIO UART unit 0, on VRQ 8, the result is also
 * printed there, then the received data is echoed back. The UART is
 * handled by the mini serial driver in ./source/vserial.c. Receiving
 * Ctrl-D stops the scheduler, main() returns and the sandbox terminates
 * with exit code zero.
 * Fatal conditions are handled by the default hooks of the port, which
 * terminate the sandbox with one of the portSB_EXIT_xxx codes.
 * In FPU builds the FPU registers are also checked, they must be preserved
 * by voluntary context switches and by VRQ preemptions.
 */

/*===========================================================================*/
/* Demo settings.                                                            */
/*===========================================================================*/

#define TEST_DURATION           pdMS_TO_TICKS(2000)
#define PERIODIC_PERIOD         pdMS_TO_TICKS(10)
#define TIMER_PERIOD            pdMS_TO_TICKS(50)
#define IDLE_DURATION           pdMS_TO_TICKS(100)
#define CATCHUP_DURATION_MS     100U
#define SWVRQ_CYCLES            100U
#define TEST_STACK_SIZE         (configMINIMAL_STACK_SIZE * 3)
#define SERIAL_STACK_SIZE       (configMINIMAL_STACK_SIZE * 3)
#define SERIAL_END_CHAR         0x04U
#define SERIAL_DRAIN_TIME       pdMS_TO_TICKS(20)
#define FPU_SPIN_DURATION_MS    200U
#define FPU_SPIN_LOOPS          20000U
#define FPU_STACK_SIZE          (configMINIMAL_STACK_SIZE * 2)

#define BLINK_PASS_PERIOD       pdMS_TO_TICKS(500)
#define BLINK_FAIL_PERIOD       pdMS_TO_TICKS(100)

/* VIO GPIO unit of the LED.*/
#define LED_VGPIO_UNIT          0U

/* Software-triggered VRQ, no suffix because it is pasted into the
   handler name by portVRQ_HANDLER().*/
#define SWVRQ                   31

/* Failure bits.*/
#define FAIL_PERIODIC           (1U << 0)
#define FAIL_TIMER              (1U << 1)
#define FAIL_QUEUE              (1U << 2)
#define FAIL_TICK_DRIFT         (1U << 3)
#define FAIL_SWVRQ              (1U << 4)
#define FAIL_IDLE_TIME          (1U << 5)
#define FAIL_IDLE_SPIN          (1U << 6)
#define FAIL_TICK_CATCHUP       (1U << 7)
#define FAIL_FPU                (1U << 8)

/*===========================================================================*/
/* Demo variables.                                                           */
/*===========================================================================*/

static QueueHandle_t xQueue;
static SemaphoreHandle_t xSwvrqSem;
static TimerHandle_t xTimer;
static TaskHandle_t xProducerHandle;
static TaskHandle_t xPeriodicHandle;
static TaskHandle_t xBlinkerHandle;
static TaskHandle_t xSerialHandle;

static char cResult[128];

static volatile uint32_t ulPeriodicCount;
static volatile uint32_t ulTimerCount;
static volatile uint32_t ulConsumerCount;
static volatile uint32_t ulSequenceErrors;
static volatile uint32_t ulSwvrqCount;
static volatile uint32_t ulIdleCount;
static volatile uint32_t ulFpuChecks;
static volatile uint32_t ulFpuSwitches;

/*===========================================================================*/
/* LED functions.                                                            */
/*===========================================================================*/

/*
 * LED blinker task, the blink period shows the self-test result.
 */
static void prvBlinkerTask(void *pvParameters) {
  uint32_t ulFailures;
  TickType_t xPeriod;

  (void)pvParameters;

  vgpioWrite(LED_VGPIO_UNIT, 1U);
  (void)xTaskNotifyWait(0U, 0U, &ulFailures, portMAX_DELAY);
  xPeriod = (ulFailures == 0U) ? BLINK_PASS_PERIOD : BLINK_FAIL_PERIOD;

  for (;;) {
    vTaskDelay(xPeriod);
    vgpioToggle(LED_VGPIO_UNIT, 1U);
  }
}

/*===========================================================================*/
/* Serial functions.                                                         */
/*===========================================================================*/

static void prvSerialPuts(const char *pcString) {

  (void)xVSerialWrite(pcString, strlen(pcString), portMAX_DELAY);
}

/*
 * Serial task, prints the self-test result then echoes the received data,
 * a line feed is added after each carriage return. The end character
 * stops the scheduler, the output is given some time to be transmitted
 * because the sandbox termination discards it.
 */
static void prvSerialTask(void *pvParameters) {
  uint8_t ucBuffer[16];
  char cErrors[32];
  uint32_t ulFailures, ulErrors;
  size_t i, xStart, xn;

  (void)pvParameters;

  prvSerialPuts("\r\nFreeRTOS sandbox, self-test running...\r\n");
  (void)xTaskNotifyWait(0U, 0U, &ulFailures, portMAX_DELAY);
  prvSerialPuts(cResult);
  prvSerialPuts((ulFailures == 0U) ? "\r\nPASSED, echo active\r\n" :
                                     "\r\nFAILED, echo active\r\n");

  for (;;) {
    xn = xVSerialRead(ucBuffer, sizeof (ucBuffer), portMAX_DELAY);

    xStart = 0U;
    for (i = 0U; i < xn; i++) {
      if (ucBuffer[i] == (uint8_t)'\r') {
        (void)xVSerialWrite(&ucBuffer[xStart], (i + 1U) - xStart,
                            portMAX_DELAY);
        prvSerialPuts("\n");
        xStart = i + 1U;
      }
      else if (ucBuffer[i] == SERIAL_END_CHAR) {
        (void)xVSerialWrite(&ucBuffer[xStart], i - xStart, portMAX_DELAY);
        prvSerialPuts("\r\nstopping the scheduler\r\n");
        vTaskDelay(SERIAL_DRAIN_TIME);
        vTaskEndScheduler();
      }
    }
    (void)xVSerialWrite(&ucBuffer[xStart], xn - xStart, portMAX_DELAY);

    ulErrors = ulVSerialGetAndClearErrors();
    if (ulErrors != 0U) {
      (void)snprintf(cErrors, sizeof (cErrors), "\r\n<errors 0x%02lx>\r\n",
                     (unsigned long)ulErrors);
      prvSerialPuts(cErrors);
    }
  }
}

/*===========================================================================*/
/* FPU check functions.                                                      */
/*===========================================================================*/

#if defined(__ARM_FP)
/*
 * FPU registers pattern, the FPSCR value selects a rounding mode.
 */
typedef struct {
  uint32_t      ulS[32];
  uint32_t      ulFpscr;
} FpuPattern_t;

static FpuPattern_t xFpuPatternTest, xFpuPatternA, xFpuPatternB;
static TaskHandle_t xFpuAHandle, xFpuBHandle;
static volatile uint32_t ulFpuErrors;

#define FPU_CLOBBERS                                                        \
  "s0", "s1", "s2", "s3", "s4", "s5", "s6", "s7",                           \
  "s8", "s9", "s10", "s11", "s12", "s13", "s14", "s15",                     \
  "s16", "s17", "s18", "s19", "s20", "s21", "s22", "s23",                   \
  "s24", "s25", "s26", "s27", "s28", "s29", "s30", "s31"

static void prvFpuPatternInit(FpuPattern_t *pxPattern, uint32_t ulId,
                              uint32_t ulFpscr) {
  unsigned i;

  for (i = 0U; i < 32U; i++) {
    pxPattern->ulS[i] = 0xA5000000U | (ulId << 16) | i;
  }
  pxPattern->ulFpscr = ulFpscr;
}

/*
 * Loads a pattern in all the FPU registers and spins, the task is
 * preempted by VRQs meanwhile, then checks the registers.
 */
static bool prvFpuSpinCheck(const FpuPattern_t *pxPattern, uint32_t ulLoops) {
  uint32_t ulS[32];
  uint32_t ulFpscr;

  __asm__ volatile ("vmsr     FPSCR, %[fpscr_in]          \n\t"
                    "vldmia   %[src], {s0-s31}            \n\t"
                    "1:                                   \n\t"
                    "subs     %[n], %[n], #1              \n\t"
                    "bne      1b                          \n\t"
                    "vstmia   %[dst], {s0-s31}            \n\t"
                    "vmrs     %[fpscr_out], FPSCR"
                    : [n] "+r" (ulLoops), [fpscr_out] "=r" (ulFpscr)
                    : [src] "r" (pxPattern->ulS), [dst] "r" (ulS),
                      [fpscr_in] "r" (pxPattern->ulFpscr)
                    : FPU_CLOBBERS, "cc", "memory");

  return (memcmp(ulS, pxPattern->ulS, sizeof (ulS)) == 0) &&
         (ulFpscr == pxPattern->ulFpscr);
}

/*
 * FPU task, loads its pattern in all the FPU registers then sleeps, the
 * callee-saved registers and FPSCR must be preserved by the switches.
 */
static void prvFpuTask(void *pvParameters) {
  const FpuPattern_t *pxPattern = (const FpuPattern_t *)pvParameters;
  uint32_t ulS[16];
  uint32_t ulFpscr;

  for (;;) {
    __asm__ volatile ("vmsr     FPSCR, %[fpscr_in]        \n\t"
                      "vldmia   %[src], {s0-s31}"
                      :
                      : [src] "r" (pxPattern->ulS),
                        [fpscr_in] "r" (pxPattern->ulFpscr)
                      : FPU_CLOBBERS, "memory");
    vTaskDelay(1U);
    __asm__ volatile ("vstmia   %[dst], {s16-s31}         \n\t"
                      "vmrs     %[fpscr_out], FPSCR"
                      : [fpscr_out] "=r" (ulFpscr)
                      : [dst] "r" (ulS)
                      : "memory");
    if ((memcmp(ulS, &pxPattern->ulS[16], sizeof (ulS)) != 0) ||
        (ulFpscr != pxPattern->ulFpscr)) {
      ulFpuErrors++;
    }
    ulFpuSwitches++;
  }
}

/*
 * FPU check, the FPU tasks load their patterns on each tick while the
 * test task spins with its own pattern.
 */
static uint32_t prvFpuCheck(void) {
  systime_t xStart;
  sysinterval_t xDuration;
  uint32_t ulFailures = 0U;

  vTaskResume(xFpuAHandle);
  vTaskResume(xFpuBHandle);

  xDuration = (sbGetFrequency() / 1000U) * FPU_SPIN_DURATION_MS;
  xStart    = sbGetSystemTime();
  while ((sysinterval_t)(sbGetSystemTime() - xStart) < xDuration) {
    if (!prvFpuSpinCheck(&xFpuPatternTest, FPU_SPIN_LOOPS)) {
      ulFailures |= FAIL_FPU;
    }
    ulFpuChecks++;
  }

  vTaskSuspend(xFpuAHandle);
  vTaskSuspend(xFpuBHandle);

  if ((ulFpuErrors != 0U) || (ulFpuSwitches == 0U)) {
    ulFailures |= FAIL_FPU;
  }

  return ulFailures;
}
#endif /* defined(__ARM_FP) */

/*===========================================================================*/
/* Self-test functions.                                                      */
/*===========================================================================*/

static bool prvInRange(uint32_t ulValue, uint32_t ulExpected,
                       uint32_t ulTolerance) {

  return (ulValue + ulTolerance >= ulExpected) &&
         (ulValue <= ulExpected + ulTolerance);
}

/*
 * VRQ handler, wakes the swvrq task which has higher priority than the
 * task triggering the VRQ.
 */
portVRQ_HANDLER(SWVRQ) {
  BaseType_t xWoken = pdFALSE;

  (void)xSemaphoreGiveFromISR(xSwvrqSem, &xWoken);
  portYIELD_FROM_ISR(xWoken);
}

static void prvTimerCallback(TimerHandle_t xTimerHandle) {

  (void)xTimerHandle;

  ulTimerCount++;
}

/*
 * Periodic task, preempts the producer on ticks.
 */
static void prvPeriodicTask(void *pvParameters) {
  TickType_t xLastWake;

  (void)pvParameters;

  xLastWake = xTaskGetTickCount();
  for (;;) {
    (void)xTaskDelayUntil(&xLastWake, PERIODIC_PERIOD);
    ulPeriodicCount++;
  }
}

/*
 * Producer task, never blocks, each send wakes the higher priority
 * consumer from within a critical zone.
 */
static void prvProducerTask(void *pvParameters) {
  uint32_t ulSeq = 0U;

  (void)pvParameters;

  for (;;) {
    (void)xQueueSend(xQueue, &ulSeq, portMAX_DELAY);
    ulSeq++;
  }
}

static void prvConsumerTask(void *pvParameters) {
  uint32_t ulExpected = 0U;
  uint32_t ulSeq;

  (void)pvParameters;

  for (;;) {
    (void)xQueueReceive(xQueue, &ulSeq, portMAX_DELAY);
    if (ulSeq != ulExpected) {
      ulSequenceErrors++;
    }
    ulExpected = ulSeq + 1U;
    ulConsumerCount++;
  }
}

static void prvSwvrqTask(void *pvParameters) {

  (void)pvParameters;

  for (;;) {
    (void)xSemaphoreTake(xSwvrqSem, portMAX_DELAY);
    ulSwvrqCount++;
  }
}

static void prvTestTask(void *pvParameters) {
  uint32_t ulFailures = 0U;
  uint32_t ulHostTicksPerTick, ulPeriodic, ulTimer, ulConsumer, ulIdle;
  systime_t xHostStart, xHostElapsed;
  TickType_t xStart, xTicks;
  unsigned i;

  (void)pvParameters;

  ulHostTicksPerTick = sbGetFrequency() / (uint32_t)configTICK_RATE_HZ;

  /* Concurrent activity, the producer runs whenever the other tasks are
     blocked.*/
  ulPeriodic = ulPeriodicCount;
  ulTimer    = ulTimerCount;
  ulConsumer = ulConsumerCount;
  xStart     = xTaskGetTickCount();
  xHostStart = sbGetSystemTime();
  vTaskDelay(TEST_DURATION);
  xHostElapsed = sbGetSystemTime() - xHostStart;
  xTicks       = xTaskGetTickCount() - xStart;
  ulPeriodic   = ulPeriodicCount - ulPeriodic;
  ulTimer      = ulTimerCount - ulTimer;
  ulConsumer   = ulConsumerCount - ulConsumer;
  vTaskSuspend(xProducerHandle);

  if (!prvInRange(ulPeriodic, TEST_DURATION / PERIODIC_PERIOD, 1U)) {
    ulFailures |= FAIL_PERIODIC;
  }
  if (!prvInRange(ulTimer, TEST_DURATION / TIMER_PERIOD, 1U)) {
    ulFailures |= FAIL_TIMER;
  }
  if ((ulConsumer == 0U) || (ulSequenceErrors != 0U)) {
    ulFailures |= FAIL_QUEUE;
  }
  if (!prvInRange(xHostElapsed, xTicks * ulHostTicksPerTick,
                  ulHostTicksPerTick)) {
    ulFailures |= FAIL_TICK_DRIFT;
  }

  /* Preemption from a VRQ handler, the swvrq task must have run before
     returning from the VRQ trigger.*/
  vPortVRQEnable(SWVRQ);
  for (i = 0U; i < SWVRQ_CYCLES; i++) {
    uint32_t ulBefore = ulSwvrqCount;

    vPortVRQTrigger(SWVRQ);
    if (ulSwvrqCount != ulBefore + 1U) {
      ulFailures |= FAIL_SWVRQ;
    }
  }

#if defined(__ARM_FP)
  /* FPU registers preservation.*/
  ulFailures |= prvFpuCheck();
#endif

  /* Idle phase, the idle hook waits for VRQs instead of spinning.*/
  ulIdle     = ulIdleCount;
  xHostStart = sbGetSystemTime();
  vTaskDelay(IDLE_DURATION);
  xHostElapsed = sbGetSystemTime() - xHostStart;
  ulIdle       = ulIdleCount - ulIdle;

  if (!prvInRange(xHostElapsed, IDLE_DURATION * ulHostTicksPerTick,
                  ulHostTicksPerTick)) {
    ulFailures |= FAIL_IDLE_TIME;
  }
  if (ulIdle > (IDLE_DURATION * 2U) + 10U) {
    ulFailures |= FAIL_IDLE_SPIN;
  }

  /* Tick catch-up, the whole sandbox is blocked in the host while the
     alarm VRQs are merged, the elapsed ticks must be recovered when the
     pending VRQ is served on return.*/
  xStart = xTaskGetTickCount();
  sbSleepMilliseconds(CATCHUP_DURATION_MS);
  xTicks = xTaskGetTickCount() - xStart;

  if (!prvInRange(xTicks, pdMS_TO_TICKS(CATCHUP_DURATION_MS), 1U)) {
    ulFailures |= FAIL_TICK_CATCHUP;
  }

  (void)snprintf(cResult, sizeof (cResult),
                 "FreeRTOS %s on %s: periodic=%lu timer=%lu consumer=%lu "
                 "idle=%lu fpu=%lu/%lu result=0x%08lx",
                 tskKERNEL_VERSION_NUMBER, portARCH_NAME,
                 (unsigned long)ulPeriodic, (unsigned long)ulTimer,
                 (unsigned long)ulConsumer, (unsigned long)ulIdle,
                 (unsigned long)ulFpuChecks, (unsigned long)ulFpuSwitches,
                 (unsigned long)ulFailures);
  printf("%s\n", cResult);

  /* Stopping the test activity, only the blinker and the serial task
     remain active.*/
  vTaskSuspend(xPeriodicHandle);
  (void)xTimerStop(xTimer, portMAX_DELAY);
  (void)xTaskNotify(xBlinkerHandle, ulFailures, eSetValueWithOverwrite);
  if (xSerialHandle != NULL) {
    (void)xTaskNotify(xSerialHandle, ulFailures, eSetValueWithOverwrite);
  }
  vTaskDelete(NULL);
}

/*===========================================================================*/
/* FreeRTOS hooks.                                                           */
/*===========================================================================*/

/*
 * Replaces the default idle hook of the port in order to count the idle
 * iterations, the CPU is still released to the host.
 */
void vApplicationIdleHook(void) {

  ulIdleCount++;
  vPortWaitForVRQ();
}

/*===========================================================================*/
/* Application entry point.                                                  */
/*===========================================================================*/

int main(void) {

  /* Allocation failures are caught by vApplicationMallocFailedHook().*/
  xQueue    = xQueueCreate(4U, sizeof (uint32_t));
  xSwvrqSem = xSemaphoreCreateBinary();
  xTimer    = xTimerCreate("timer", TIMER_PERIOD, pdTRUE, NULL,
                           prvTimerCallback);
  (void)xTimerStart(xTimer, 0U);

  (void)xTaskCreate(prvSwvrqTask, "swvrq", configMINIMAL_STACK_SIZE,
                    NULL, 5U, NULL);
  (void)xTaskCreate(prvTestTask, "test", TEST_STACK_SIZE,
                    NULL, 4U, NULL);
  (void)xTaskCreate(prvPeriodicTask, "periodic", configMINIMAL_STACK_SIZE,
                    NULL, 3U, &xPeriodicHandle);
  (void)xTaskCreate(prvBlinkerTask, "blinker", configMINIMAL_STACK_SIZE,
                    NULL, 3U, &xBlinkerHandle);

  /* The serial task only exists if the host assigned the UART.*/
  if (xVSerialStart() == pdPASS) {
    (void)xTaskCreate(prvSerialTask, "serial", SERIAL_STACK_SIZE,
                      NULL, 3U, &xSerialHandle);
  }

  (void)xTaskCreate(prvConsumerTask, "consumer", configMINIMAL_STACK_SIZE,
                    NULL, 2U, NULL);
  (void)xTaskCreate(prvProducerTask, "producer", configMINIMAL_STACK_SIZE,
                    NULL, 1U, &xProducerHandle);

#if defined(__ARM_FP)
  /* The FPU tasks are resumed by the FPU check, they have the same
     priority of the swvrq task, higher than the test task.*/
  prvFpuPatternInit(&xFpuPatternTest, 0U, 0x00C00000U);
  prvFpuPatternInit(&xFpuPatternA, 1U, 0x00400000U);
  prvFpuPatternInit(&xFpuPatternB, 2U, 0x00800000U);
  (void)xTaskCreate(prvFpuTask, "fpu_a", FPU_STACK_SIZE,
                    &xFpuPatternA, 5U, &xFpuAHandle);
  (void)xTaskCreate(prvFpuTask, "fpu_b", FPU_STACK_SIZE,
                    &xFpuPatternB, 5U, &xFpuBHandle);
  vTaskSuspend(xFpuAHandle);
  vTaskSuspend(xFpuBHandle);
#endif

  vTaskStartScheduler();

  /* Reached after vTaskEndScheduler(), the returned value is the sandbox
     exit code. Failures creating the idle or timer task are caught by
     vApplicationMallocFailedHook().*/
  return 0;
}
