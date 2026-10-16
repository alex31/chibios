/*
    ChibiOS - Copyright (C) 2006-2026 Giovanni Di Sirio.

    This file is part of ChibiOS.

    ChibiOS is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation version 3 of the License.

    ChibiOS is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/**
 * @file    ARMvx-M-SB/chcore.c
 * @brief   ARMvx-M-SB port code.
 *
 * @addtogroup ARMVXM_SB_CORE
 * @{
 */

#include "ch.h"

/*===========================================================================*/
/* Module local definitions.                                                 */
/*===========================================================================*/

/*===========================================================================*/
/* Module exported variables.                                                */
/*===========================================================================*/

/*===========================================================================*/
/* Module local types.                                                       */
/*===========================================================================*/

/*===========================================================================*/
/* Module local variables.                                                   */
/*===========================================================================*/

#if PORT_USE_LOCAL_SYSTICK == TRUE
/**
 * @brief   Tick period in host system time units.
 */
static sysinterval_t tick_interval;

/**
 * @brief   Host system time of the last accounted tick.
 */
static uint32_t last_tick;
#endif

/*===========================================================================*/
/* Module local functions.                                                   */
/*===========================================================================*/

/*===========================================================================*/
/* Module interrupt handlers.                                                */
/*===========================================================================*/

#if PORT_USE_LOCAL_SYSTICK == TRUE
/**
 * @brief   Alarm VRQ handler.
 * @details All the tick periods elapsed since the last accounted tick are
 *          accounted. The host merges the alarm VRQs triggered while the
 *          sandbox is not running, those ticks are recovered here.
 *
 * @isr
 */
CH_IRQ_HANDLER(__sb_vector0) {
  uint32_t now;

  CH_IRQ_PROLOGUE();

  now = (uint32_t)sbGetSystemTime();

  /* A negative difference is only possible with a host system time
     narrower than 32 bits, the elapsed ticks are lost in that case.*/
  if ((int32_t)(now - last_tick) < 0) {
    last_tick = now;
  }

  chSysLockFromISR();
  while ((now - last_tick) >= (uint32_t)tick_interval) {
    last_tick += (uint32_t)tick_interval;
    chSysTimerHandlerI();
  }
  chSysUnlockFromISR();

  CH_IRQ_EPILOGUE();
}
#endif /* PORT_USE_LOCAL_SYSTICK == TRUE */

/*===========================================================================*/
/* Module exported functions.                                                */
/*===========================================================================*/

/**
 * @brief   Port-related initialization code.
 *
 * @param[in, out] oip  pointer to the @p os_instance_t structure
 *
 * @notapi
 */
void port_init(os_instance_t *oip) {

  (void)oip;

  /* Starting in a known IRQ configuration.*/
  port_disable();

#if PORT_USE_LOCAL_SYSTICK == TRUE
  tick_interval = sbGetFrequency() / CH_CFG_ST_FREQUENCY;

  chDbgAssert(tick_interval > (sysinterval_t)0, "invalid frequency");

  /* Enabling alarm VRQ, the tick accounting base is taken before arming
     the alarm.*/
  last_tick = (uint32_t)sbGetSystemTime();
  __sb_vrq_seten(1U << 0);
  sbSetAlarm(tick_interval, true);
#endif
}

/** @} */
