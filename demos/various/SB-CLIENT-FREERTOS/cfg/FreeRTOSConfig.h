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

/**
 * @file    FreeRTOSConfig.h
 * @brief   FreeRTOS configuration for a sandbox guest.
 */

#ifndef FREERTOSCONFIG_H
#define FREERTOSCONFIG_H

/*===========================================================================*/
/* Scheduler settings.                                                       */
/*===========================================================================*/

#define configUSE_PREEMPTION                    1
#define configUSE_TIME_SLICING                  1
#define configUSE_PORT_OPTIMISED_TASK_SELECTION 1
#define configUSE_TICKLESS_IDLE                 0
#define configTICK_RATE_HZ                      1000
#define configMAX_PRIORITIES                    8
#define configMINIMAL_STACK_SIZE                128
#define configMAX_TASK_NAME_LEN                 12
#define configTICK_TYPE_WIDTH_IN_BITS           TICK_TYPE_WIDTH_32_BITS
#define configIDLE_SHOULD_YIELD                 1
#define configTASK_NOTIFICATION_ARRAY_ENTRIES   1
#define configQUEUE_REGISTRY_SIZE               0
#define configENABLE_BACKWARD_COMPATIBILITY     0
#define configNUM_THREAD_LOCAL_STORAGE_POINTERS 0
#define configUSE_NEWLIB_REENTRANT              0

/*===========================================================================*/
/* Memory allocation settings.                                               */
/*===========================================================================*/

#define configSUPPORT_STATIC_ALLOCATION         0
#define configSUPPORT_DYNAMIC_ALLOCATION        1
#define configTOTAL_HEAP_SIZE                   (9U * 1024U)
#define configAPPLICATION_ALLOCATED_HEAP        0

/*===========================================================================*/
/* Hook settings.                                                            */
/*===========================================================================*/

/* The idle hook releases the CPU to the sandbox host, the other enabled
   hooks terminate the sandbox, default implementations are provided by the
   port.*/
#define configUSE_IDLE_HOOK                     1
#define configUSE_TICK_HOOK                     0
#define configUSE_MALLOC_FAILED_HOOK            1
#define configUSE_DAEMON_TASK_STARTUP_HOOK      0
#define configCHECK_FOR_STACK_OVERFLOW          2

/*===========================================================================*/
/* Run time and task stats settings.                                         */
/*===========================================================================*/

#define configGENERATE_RUN_TIME_STATS           0
#define configUSE_TRACE_FACILITY                0
#define configUSE_STATS_FORMATTING_FUNCTIONS    0

/*===========================================================================*/
/* Primitives settings.                                                      */
/*===========================================================================*/

#define configUSE_TASK_NOTIFICATIONS            1
#define configUSE_MUTEXES                       1
#define configUSE_RECURSIVE_MUTEXES             0
#define configUSE_COUNTING_SEMAPHORES           1
#define configUSE_QUEUE_SETS                    0
#define configUSE_CO_ROUTINES                   0

/*===========================================================================*/
/* Software timers settings.                                                 */
/*===========================================================================*/

#define configUSE_TIMERS                        1
#define configTIMER_TASK_PRIORITY               (configMAX_PRIORITIES - 1)
#define configTIMER_QUEUE_LENGTH                4
#define configTIMER_TASK_STACK_DEPTH            (configMINIMAL_STACK_SIZE * 2)

/*===========================================================================*/
/* Debug settings.                                                           */
/*===========================================================================*/

/* The port provides a default vAssertCalled() terminating the sandbox.*/
#if !defined(__ASSEMBLER__)
void vAssertCalled(const char *pcFile, unsigned long ulLine);
#endif

#define configASSERT(x)                                                     \
  do {                                                                      \
    if ((x) == 0) {                                                         \
      vAssertCalled(__FILE__, __LINE__);                                    \
    }                                                                       \
  } while (0)

/*===========================================================================*/
/* Optional functions.                                                       */
/*===========================================================================*/

#define INCLUDE_vTaskPrioritySet                1
#define INCLUDE_uxTaskPriorityGet               1
#define INCLUDE_vTaskDelete                     1
#define INCLUDE_vTaskSuspend                    1
#define INCLUDE_xTaskDelayUntil                 1
#define INCLUDE_vTaskDelay                      1
#define INCLUDE_xTaskGetSchedulerState          1
#define INCLUDE_xTaskGetCurrentTaskHandle       1
#define INCLUDE_uxTaskGetStackHighWaterMark     1

#endif /* FREERTOSCONFIG_H */
