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
 * @file    sandbox/hal_st_lld.c
 * @brief   ST Driver subsystem low level driver code.
 *
 * @addtogroup ST
 * @{
 */

#include "hal.h"

/*===========================================================================*/
/* Driver local definitions.                                                 */
/*===========================================================================*/

/*===========================================================================*/
/* Driver exported variables.                                                */
/*===========================================================================*/

/*===========================================================================*/
/* Driver local types.                                                       */
/*===========================================================================*/

/*===========================================================================*/
/* Driver local variables and types.                                         */
/*===========================================================================*/

#if (PORT_USE_LOCAL_SYSTICK == FALSE) && (CH_CFG_ST_TIMEDELTA == 0)
/**
 * @brief   Tick period in host system time units.
 */
static sysinterval_t st_alarm_interval;

/**
 * @brief   Host system time of the last accounted tick.
 */
static uint32_t st_last_tick;
#endif

/*===========================================================================*/
/* Driver local functions.                                                   */
/*===========================================================================*/

/*===========================================================================*/
/* Driver interrupt handlers.                                                */
/*===========================================================================*/

/**
 * @brief   Interrupt handler.
 * @details All the tick periods elapsed since the last accounted tick are
 *          accounted. The host merges the alarm VRQs triggered while the
 *          sandbox is not running, those ticks are recovered here.
 *
 * @isr
 */
#if (PORT_USE_LOCAL_SYSTICK == FALSE) && (CH_CFG_ST_TIMEDELTA == 0)
CH_IRQ_HANDLER(MK_VECTOR(SB_VRQ_ALARM)) {
  uint32_t now;

  CH_IRQ_PROLOGUE();

  now = (uint32_t)sbGetSystemTime();

  /* A negative difference is only possible with a host system time
     narrower than 32 bits, the elapsed ticks are lost in that case.*/
  if ((int32_t)(now - st_last_tick) < 0) {
    st_last_tick = now;
  }

  chSysLockFromISR();
  while ((now - st_last_tick) >= (uint32_t)st_alarm_interval) {
    st_last_tick += (uint32_t)st_alarm_interval;
    chSysTimerHandlerI();
  }
  chSysUnlockFromISR();

  CH_IRQ_EPILOGUE();
}
#endif

/*===========================================================================*/
/* Driver exported functions.                                                */
/*===========================================================================*/

/**
 * @brief   Low level ST driver initialization.
 *
 * @notapi
 */
void st_lld_init(void) {

#if (PORT_USE_LOCAL_SYSTICK == FALSE) && (CH_CFG_ST_TIMEDELTA == 0)
  st_alarm_interval = sbGetFrequency() / CH_CFG_ST_FREQUENCY;

  chDbgAssert(st_alarm_interval > (sysinterval_t)0, "invalid frequency");

  /* The ST layer owns the alarm VRQ when the port-local systick is
     disabled. The tick accounting base is taken before arming the
     alarm.*/
  st_last_tick = (uint32_t)sbGetSystemTime();
  __sb_vrq_seten(1U << SB_VRQ_ALARM);
  sbSetAlarm(st_alarm_interval, true);
#endif
}

/** @} */
