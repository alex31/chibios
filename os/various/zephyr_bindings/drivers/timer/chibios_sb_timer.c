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
 * @file    drivers/timer/chibios_sb_timer.c
 * @brief   ChibiOS sandbox system timer.
 * @details The counter is the host system time, the timer is the host
 *          alarm delivered on @p SB_VRQ_ALARM. The alarm is periodic, it
 *          matches the auto-reload backend of the generic timer core. The
 *          host merges the alarms expired while the sandbox is not
 *          running, the elapsed ticks are recovered from the counter.
 *
 * @addtogroup ZEPHYR_CHIBIOS_SB
 * @{
 */

#include <zephyr/init.h>
#include <zephyr/irq.h>
#include <zephyr/sys/util.h>

/*===========================================================================*/
/* External declarations.                                                    */
/*===========================================================================*/

/* Kernel variable holding the run time frequency.*/
extern unsigned int z_clock_hw_cycles_per_sec;

/*===========================================================================*/
/* Module local functions.                                                   */
/*===========================================================================*/

static uint32_t timer_driver_cycle_get(void) {

  return chibios_sb_get_systime();
}

static void timer_driver_set_reload(uint32_t cycles) {

  (void)chibios_sb_set_alarm(cycles, true);
}

/* Generic timer core settings, the alarm interval is limited to half the
   host system time range.*/
#define TIMER_CORE_BACKEND_RELOAD
#define TIMER_CORE_COUNTER_WIDTH        32
#define TIMER_CORE_ALARM_MAX_CYCLES     0x7FFFFFFFU

#include "system_timer_generic.h"

static void chibios_sb_timer_isr(const void *arg) {

  ARG_UNUSED(arg);

  timer_core_announce();
}

static int sys_clock_driver_init(void) {

  /* The host system time frequency is only known at run time.*/
  z_clock_hw_cycles_per_sec = chibios_sb_get_frequency();

  IRQ_CONNECT(SB_VRQ_ALARM, 0, chibios_sb_timer_isr, NULL, 0);
  irq_enable(SB_VRQ_ALARM);

  timer_core_init();

  /* Without a tickless kernel the alarm is armed once, one tick period,
     and never reprogrammed.*/
  if (!IS_ENABLED(CONFIG_TICKLESS_KERNEL)) {
    timer_driver_set_reload(TIMER_CORE_CYC_PER_TICK);
  }

  return 0;
}

/*===========================================================================*/
/* Module exported functions.                                                */
/*===========================================================================*/

/**
 * @brief   Stops the system timer.
 */
void sys_clock_disable(void) {

  irq_disable(SB_VRQ_ALARM);
  chibios_sb_reset_alarm();
}

SYS_INIT(sys_clock_driver_init, PRE_KERNEL_2,
         CONFIG_SYSTEM_CLOCK_INIT_PRIORITY);

/** @} */
