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
 * @file    ARMv8-M-ML-ALT/smp/rp2/chcoresmp.c
 * @brief   ARMv8-M-ML-ALT RP2 SMP code.
 *
 * @addtogroup ARMV8M_ML_ALT_CORE_SMP_RP2
 * @{
 */

#include "ch.h"

#if (CH_CFG_SMP_MODE == TRUE) || defined(__DOXYGEN__)

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

/**
 * @brief   Cores which completed SMP initialization.
 * @note    A core which never started must not be waited for in the
 *          lockout handshake.
 */
static volatile bool port_lockout_ready[PORT_CORES_NUMBER];

/*===========================================================================*/
/* Module local functions.                                                   */
/*===========================================================================*/

static void port_local_halt(void) {
  const char *reason = "remote panic";

  port_disable();

  __trace_halt("remote panic");

  currcore->dbg.panic_msg = reason;

  CH_CFG_SYSTEM_HALT_HOOK(reason);

  while (true) {
  }
}

/**
 * @brief   Parks the core while the other core has flash unavailable.
 * @details Called from the FIFO handler on reception of the lockout token.
 *          The whole wait executes from RAM with interrupts masked because
 *          the requesting core is about to disable XIP; any flash fetch on
 *          this core would fault or return garbage.
 */
CC_NO_INLINE CC_SECTION(".ramtext")
static void port_fifo_lockout_wait(void) {
  uint32_t primask = __get_PRIMASK();

  __disable_irq();

  /* Acknowledging the lockout, the requester waits for this before
     touching XIP.*/
  while ((SIO->FIFO_ST & SIO_FIFO_ST_RDY) == 0U) {
  }
  SIO->FIFO_WR = PORT_FIFO_LOCKOUT_ACK_MESSAGE;
  __SEV();

  /* Spinning on SIO registers only until released.*/
  while (true) {
    uint32_t message;

    while ((SIO->FIFO_ST & SIO_FIFO_ST_VLD) == 0U) {
    }
    message = SIO->FIFO_RD;
    if (message == PORT_FIFO_UNLOCK_MESSAGE) {
      break;
    }
    if (message == PORT_FIFO_PANIC_MESSAGE) {
      /* Cannot reach the flash-resident halt path, parking here, the
         other core is halting anyway.*/
      while (true) {
      }
    }
    /* Anything else (reschedule tokens) is stale, the ISR epilogue
       reschedules on return anyway.*/
  }

  /* Acknowledging the unlock, XIP is available again at this point.*/
  while ((SIO->FIFO_ST & SIO_FIFO_ST_RDY) == 0U) {
  }
  SIO->FIFO_WR = PORT_FIFO_LOCKOUT_ACK_MESSAGE;
  __SEV();

  __set_PRIMASK(primask);
}

/**
 * @brief   Sends a token to the other core and waits for its acknowledge.
 * @details The FIFO RX side is drained directly because this core's FIFO
 *          interrupt is masked during the handshake.
 *
 * @param[in] token     token to be sent
 * @return              @p true on acknowledge, @p false on timeout.
 */
static bool port_lockout_handshake(uint32_t token) {
  uint32_t start = TIMER0->TIMERAWL;

  while ((SIO->FIFO_ST & SIO_FIFO_ST_RDY) == 0U) {
    if ((TIMER0->TIMERAWL - start) > PORT_LOCKOUT_TIMEOUT_US) {
      return false;
    }
  }
  SIO->FIFO_WR = token;
  __SEV();

  while (true) {
    if ((SIO->FIFO_ST & SIO_FIFO_ST_VLD) != 0U) {
      uint32_t message = SIO->FIFO_RD;

      if (message == PORT_FIFO_LOCKOUT_ACK_MESSAGE) {
        return true;
      }
      if (message == PORT_FIFO_PANIC_MESSAGE) {
        port_local_halt();
      }
      /* Reschedule tokens are dropped here, a reschedule round is forced
         after the handshake.*/
    }
    if ((TIMER0->TIMERAWL - start) > PORT_LOCKOUT_TIMEOUT_US) {
      return false;
    }
  }
}

/*===========================================================================*/
/* Module interrupt handlers.                                                */
/*===========================================================================*/

/**
 * @brief   Single FIFO interrupt handler for both cores (RP2350).
 * @note    RP2350 uses a shared SIO_IRQ_FIFO (IRQ 25) unlike RP2040 which
 *          has separate IRQs per core.
 *
 * @isr
 */
CH_IRQ_HANDLER(VectorA4) {

  CH_IRQ_PROLOGUE();

  SIO->FIFO_ST = SIO_FIFO_ST_ROE | SIO_FIFO_ST_WOF;

  while ((SIO->FIFO_ST & SIO_FIFO_ST_VLD) != 0U) {
    uint32_t message = SIO->FIFO_RD;
    /* A panic on either core halts the other one too.*/
    if (message == PORT_FIFO_PANIC_MESSAGE) {
      port_local_halt();
    }
    /* The other core needs this core off flash, parking until released.*/
    if (message == PORT_FIFO_LOCKOUT_MESSAGE) {
      port_fifo_lockout_wait();
      continue;
    }
#if defined(PORT_HANDLE_FIFO_MESSAGE)
    if (message < PORT_FIFO_LOCKOUT_ACK_MESSAGE) {
      PORT_HANDLE_FIFO_MESSAGE(port_get_core_id() ^ 1U, message);
    }
#else
    (void)message;
#endif
  }

  __SEV();

  CH_IRQ_EPILOGUE();
}

/*===========================================================================*/
/* Module exported functions.                                                */
/*===========================================================================*/

/**
 * @brief   SMP-related port initialization.
 *
 * @param[in, out] oip  pointer to the @p os_instance_t structure
 */
void __port_smp_init(os_instance_t *oip) {

#if CH_CFG_ST_TIMEDELTA > 0
  /* Activating timer for this instance.*/
  port_timer_enable(oip);
#endif

  /* FIFO handler for this core. RP2350 uses a single shared IRQ 25.*/
  SIO->FIFO_ST = SIO_FIFO_ST_ROE | SIO_FIFO_ST_WOF;
  NVIC_SetPriority(SIO_IRQ_FIFOn, CORTEX_MINIMUM_PRIORITY);
  NVIC_EnableIRQ(SIO_IRQ_FIFOn);

  /* This core can now be parked by the other one.*/
  port_lockout_ready[port_get_core_id()] = true;

  (void)oip;
}

/**
 * @brief   Parks the other core outside flash and masks local interrupts
 *          sourced from it.
 * @details On return the other core is spinning in RAM with interrupts
 *          disabled and stays there until @p __port_flash_unlockout() is
 *          called. Requests from both cores are serialized on a dedicated
 *          hardware spinlock which is spun with interrupts enabled so that
 *          a crossing request from the other core can park this core
 *          first instead of deadlocking.
 * @note    Must be called from thread context outside any critical
 *          section.
 */
void __port_flash_lockout(void) {

  chDbgAssert(!port_is_isr_context() &&
              __port_irq_enabled(__port_get_irq_status()),
              "not in thread context");

  /* Serializing requesters, interrupts stay enabled while spinning.*/
  while (SIO->SPINLOCK[PORT_LOCKOUT_SPINLOCK_NUMBER] == 0U) {
  }
  __DMB();

  /* A core which never initialized cannot acknowledge and does not need
     parking.*/
  if (!port_lockout_ready[port_get_core_id() ^ 1U]) {
    return;
  }

  /* Masking local interrupts so that the FIFO handler cannot steal the
     acknowledge token; no lockout traffic can be in flight here because
     the spinlock is held.*/
  __disable_irq();

  if (!port_lockout_handshake(PORT_FIFO_LOCKOUT_MESSAGE)) {
    chSysHalt("lockout timeout");
  }

  __enable_irq();
}

/**
 * @brief   Releases the core parked by @p __port_flash_lockout().
 */
void __port_flash_unlockout(void) {

  chDbgAssert((SIO->SPINLOCK_ST &
               (1UL << PORT_LOCKOUT_SPINLOCK_NUMBER)) != 0U,
              "lockout not active");

  if (port_lockout_ready[port_get_core_id() ^ 1U]) {
    __disable_irq();

    if (!port_lockout_handshake(PORT_FIFO_UNLOCK_MESSAGE)) {
      chSysHalt("unlock timeout");
    }

    __enable_irq();
  }

  __DMB();
  SIO->SPINLOCK[PORT_LOCKOUT_SPINLOCK_NUMBER] = (uint32_t)SIO;

  /* A reschedule token could have been consumed during the handshakes,
     forcing a reschedule round.*/
  chSysLock();
  chSchRescheduleS();
  chSysUnlock();
}

/**
 * @brief   Takes the kernel spinlock.
 */
void __port_spinlock_take(void) {

  port_spinlock_take();
}

/**
 * @brief   Releases the kernel spinlock.
 */
void __port_spinlock_release(void) {

  port_spinlock_release();
}

#endif /* CH_CFG_SMP_MODE == TRUE */

/** @} */
