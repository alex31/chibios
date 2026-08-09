/*
    ChibiOS - Copyright (C) 2022 Stefan Kerkmann.
    ChibiOS - Copyright (C) 2021 Hanya.
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
 * @file    I2Cv1/hal_i2c_lld.c
 * @brief   RP I2C subsystem low level driver source.
 *
 * @addtogroup I2C
 * @{
 */

#include "hal.h"

#if (HAL_USE_I2C == TRUE) || defined(__DOXYGEN__)

/*===========================================================================*/
/* Driver local definitions.                                                 */
/*===========================================================================*/

#define I2C_OVERRUN_ERRORS                                                  \
  (I2C_IC_INTR_STAT_R_RX_OVER | I2C_IC_INTR_STAT_R_RX_UNDER |               \
   I2C_IC_INTR_STAT_R_TX_OVER)

#define I2C_ERROR_INTERRUPTS                                                \
  (I2C_IC_INTR_MASK_M_TX_ABRT | I2C_IC_INTR_MASK_M_TX_OVER |                \
   I2C_IC_INTR_MASK_M_RX_OVER | I2C_IC_INTR_MASK_M_RX_UNDER)

/**
 * @brief   Half period used by the bus clear procedure, in microseconds.
 * @note    Rounded up to at least one system tick, bus recovery has no
 *          minimum speed requirement.
 */
#define I2C_BUS_CLEAR_HALF_PERIOD_US        10U

/**
 * @brief   Bound of a single SCL rise wait of the bus clear procedure,
 *          in half period sleeps.
 * @details A device may legally stretch the SCL low phase, the wait
 *          therefore paces bounded sleeps instead of sampling once. A
 *          clock line that does not rise within the bound is considered
 *          held by a failed device, such a bus cannot be recovered by
 *          pulsing SCL from this side.
 */
#define I2C_BUS_CLEAR_SCL_RISE_CYCLES       100U

/**
 * @brief   Bound of the controller enable handshake, in status polling
 *          iterations.
 * @details The databook bounds the propagation of an enable or disable
 *          request to the ENABLE_STATUS register to two ic_clk cycles
 *          for an idle controller, ic_clk being clk_sys on the RP
 *          ports. The handshake is only ever performed with the
 *          controller idle, enforced by the transfer start gate. Each
 *          polling iteration costs at least one APB status read of
 *          several clk_sys cycles, the bound is therefore orders of
 *          magnitude above the documented latency and only exhausted by
 *          misbehaving hardware.
 */
#define I2C_ENABLE_HANDSHAKE_ITERATIONS     32U

/*===========================================================================*/
/* Driver exported variables.                                                */
/*===========================================================================*/

/**
 * @brief   I2C0 driver identifier.
 */
#if (RP_I2C_USE_I2C0 == TRUE) || defined(__DOXYGEN__)
hal_i2c_driver_c I2CD0;
#endif

/**
 * @brief   I2C1 driver identifier.
 */
#if (RP_I2C_USE_I2C1 == TRUE) || defined(__DOXYGEN__)
hal_i2c_driver_c I2CD1;
#endif

/*===========================================================================*/
/* Driver local variables and types.                                         */
/*===========================================================================*/

/**
 * @brief   Driver default configuration.
 */
static const hal_i2c_config_t i2c_default_config = I2C_DEFAULT_CONFIGURATION;

/*===========================================================================*/
/* Driver local functions.                                                   */
/*===========================================================================*/

/**
 * @brief   Opens a new transfer generation.
 * @details The generation counter becomes odd, marking a transfer in
 *          flight with an unclaimed terminal event.
 * @note    Must be called with the system lock held, the transfer start
 *          methods are invoked in I-class context by the shared driver.
 *
 * @param[in] i2cp      pointer to the @p hal_i2c_driver_c object
 */
static void i2c_lld_tgen_open(hal_i2c_driver_c *i2cp) {

  chDbgAssert((i2cp->tgen & 1U) == 0U, "transfer already in flight");

  i2cp->tgen++;
}

/**
 * @brief   Returns the resets controller mask of an I2C instance.
 *
 * @param[in] i2cp      pointer to the @p hal_i2c_driver_c object
 * @return              The reset mask.
 */
static uint32_t i2c_lld_reset_mask(hal_i2c_driver_c *i2cp) {
  uint32_t mask = 0U;

  if (false) {
  }
#if RP_I2C_USE_I2C0 == TRUE
  else if (&I2CD0 == i2cp) {
    mask = RESETS_ALLREG_I2C0;
  }
#endif
#if RP_I2C_USE_I2C1 == TRUE
  else if (&I2CD1 == i2cp) {
    mask = RESETS_ALLREG_I2C1;
  }
#endif
  else {
    chDbgAssert(false, "invalid I2C instance");
  }

  return mask;
}

/**
 * @brief   I2C deactivation.
 * @details Disables the peripheral vector and puts the peripheral back
 *          in reset. Shared by the stop path and by the start failure
 *          rollback.
 *
 * @param[in] i2cp      pointer to the @p hal_i2c_driver_c object
 */
static void i2c_lld_deactivate(hal_i2c_driver_c *i2cp) {

  if (false) {
  }
#if RP_I2C_USE_I2C0 == TRUE
  else if (&I2CD0 == i2cp) {
    nvicDisableVector(RP_I2C0_IRQ_NUMBER);
    rp_peripheral_reset(RESETS_ALLREG_I2C0);
  }
#endif
#if RP_I2C_USE_I2C1 == TRUE
  else if (&I2CD1 == i2cp) {
    nvicDisableVector(RP_I2C1_IRQ_NUMBER);
    rp_peripheral_reset(RESETS_ALLREG_I2C1);
  }
#endif
  else {
    chDbgAssert(false, "invalid I2C instance");
  }
}

/**
 * @brief   Waits for the controller enable state to settle.
 * @details The ENABLE register only carries the request, the effective
 *          state is reported by the ENABLE_STATUS register after the
 *          documented propagation latency. Register accesses gated on
 *          the enable state, first of all the target address register,
 *          are only legal after the status has confirmed the requested
 *          state.
 * @note    Called with the system lock held, the poll is bounded, see
 *          @p I2C_ENABLE_HANDSHAKE_ITERATIONS.
 *
 * @param[in] dp        pointer to the registers block
 * @param[in] state     expected IC_EN state of the ENABLE_STATUS
 *                      register
 * @return              The handshake outcome.
 * @retval true         if the controller reached the expected state.
 * @retval false        if the poll bound was exhausted.
 */
static bool i2c_lld_wait_enable_state(I2C_TypeDef *dp, uint32_t state) {
  unsigned i;

  for (i = 0U; i < I2C_ENABLE_HANDSHAKE_ITERATIONS; i++) {
    if ((dp->ENABLESTATUS & I2C_IC_ENABLE_STATUS_IC_EN) == state) {
      return true;
    }
  }

  return false;
}

/**
 * @brief   Programs the target address and starts the transfer engine.
 * @details The target address register is gated on the ENABLE register
 *          bit, cycling the enable also flushes both FIFOs from any
 *          residue of a previously aborted transfer. Both edges of the
 *          enable cycle are confirmed through the ENABLE_STATUS
 *          handshake mandated by the databook before the dependent
 *          accesses are performed: the target address is written only
 *          on a confirmed disable, the interrupt sources are armed only
 *          on a confirmed enable.
 * @note    Called with the system lock held. The FIFO engine runs
 *          entirely in the interrupt service: with TX_EMPTY_CTRL
 *          selected the first TX_EMPTY interrupt fires as soon as the
 *          mask is armed because the TX FIFO is empty and no command is
 *          in flight, the service then queues data or read commands.
 *
 * @param[in] i2cp      pointer to the @p hal_i2c_driver_c object
 * @param[in] addr      slave device address
 * @return              The operation status.
 * @retval HAL_RET_SUCCESS     if the transfer engine has been started.
 * @retval HAL_RET_HW_BUSY     if an enable handshake bound was
 *                             exhausted, the peripheral is left masked
 *                             and the caller propagates the busy
 *                             condition.
 */
static msg_t i2c_lld_setup_transfer(hal_i2c_driver_c *i2cp, i2caddr_t addr) {
  I2C_TypeDef *dp = i2cp->i2c;

  /* All interrupt sources masked during the setup, this is also the
     state left behind on a failed handshake.*/
  dp->INTRMASK = 0U;

  dp->ENABLE = 0U;
  if (!i2c_lld_wait_enable_state(dp, 0U)) {
    return HAL_RET_HW_BUSY;
  }

  dp->TAR = (uint32_t)addr & I2C_IC_TAR_IC_TAR;

  dp->ENABLE = I2C_IC_ENABLE_ENABLE;
  if (!i2c_lld_wait_enable_state(dp, I2C_IC_ENABLE_STATUS_IC_EN)) {
    return HAL_RET_HW_BUSY;
  }

  /* Stale flags cleared, this also releases the TX FIFO from the
     flushed state following an abort.*/
  (void)dp->CLRINTR;

  /* Interrupt sources armed, the transfer proceeds in the service
     routines from here.*/
  dp->INTRMASK = I2C_IC_INTR_MASK_M_STOP_DET |
                 I2C_IC_INTR_MASK_M_TX_EMPTY |
                 I2C_ERROR_INTERRUPTS;

  return HAL_RET_SUCCESS;
}

/**
 * @brief   Transfer start gate.
 * @details A transfer start is rejected while the controller can still
 *          be processing a previous operation. The asynchronous abort
 *          initiated by a transfer stop is tracked in software because
 *          the ACTIVITY status only reflects the current state of the
 *          transfer state machine: it can read idle after the aborted
 *          transfer left the wire while the ABORT request is still
 *          flushing the FIFO engine. The tracking is released when the
 *          abort completion interrupt has been observed or, here, when
 *          the ABORT request is read back as self-cleared, whichever
 *          comes first.
 * @note    Called with the system lock held from I-class context,
 *          waiting is not allowed, a busy condition is reported to the
 *          caller.
 *
 * @param[in] i2cp      pointer to the @p hal_i2c_driver_c object
 * @return              The gate status.
 * @retval HAL_RET_SUCCESS     if a transfer can be started.
 * @retval HAL_RET_HW_BUSY     if the controller is busy.
 */
static msg_t i2c_lld_check_startable(hal_i2c_driver_c *i2cp) {
  I2C_TypeDef *dp = i2cp->i2c;

  if (i2cp->abort_pending) {
    if ((dp->ENABLE & I2C_IC_ENABLE_ABORT) != 0U) {
      return HAL_RET_HW_BUSY;
    }

    /* The abort request self-cleared, the latched abort flags are
       consumed by the enable cycle of the transfer setup or silenced
       by the error service which finds the generation closed.*/
    i2cp->abort_pending = false;
  }

  if ((dp->STATUS & I2C_IC_STATUS_ACTIVITY) != 0U) {
    return HAL_RET_HW_BUSY;
  }

  return HAL_RET_SUCCESS;
}

/**
 * @brief   Requests data to be received, actual reception is done in the
 *          interrupt handler.
 * @details The read command queued last carries the STOP bit and marks
 *          the transfer as stop-expected, anchoring the own-completion
 *          evidence used by the STOP detection service.
 * @note    Called with the system lock held from ISR context.
 *
 * @param[in] i2cp      pointer to the @p hal_i2c_driver_c object
 */
static void i2c_lld_request_data(hal_i2c_driver_c *i2cp) {
  I2C_TypeDef *dp = i2cp->i2c;
  uint32_t data = I2C_IC_DATA_CMD_CMD;

  /* RP Designware I2C peripheral has FIFO depth of 16 elements. As we
     specify that the TX_EMPTY interrupt only fires if the TX FIFO is
     truly empty we don't need to check the current fill level.*/
  uint32_t batch = i2cp->rxbytes < 16U ? (uint32_t)i2cp->rxbytes : 16U;

  if (i2cp->send_restart) {
    data |= I2C_IC_DATA_CMD_RESTART;
    i2cp->send_restart = false;
  }

  /* Setup RX FIFO trigger level to only trigger when the batch has been
     completely received. Therefore we don't need to check if there are
     any bytes still pending to be received.*/
  dp->RXTL = batch > 1U ? batch - 1U : 0U;

  while (batch > 0U) {
    /* Send STOP after last byte.*/
    if (i2cp->rxbytes == 1U) {
      data |= I2C_IC_DATA_CMD_STOP;
      i2cp->stop_expected = true;
    }
    dp->DATACMD = data;

    batch--;
    i2cp->rxbytes--;
    data = I2C_IC_DATA_CMD_CMD;
  }

  /* Clear TX FIFO empty interrupt, it will be re-activated when data
     has been received. Enable RX FULL interrupt to process received
     data.*/
  dp->CLR.INTRMASK = I2C_IC_INTR_MASK_M_TX_EMPTY;
  dp->SET.INTRMASK = I2C_IC_INTR_MASK_M_RX_FULL;
}

/**
 * @brief   Fills TX FIFO with data to be sent.
 * @details The data byte queued last in a pure write carries the STOP
 *          bit and marks the transfer as stop-expected, anchoring the
 *          own-completion evidence used by the STOP detection service.
 * @note    Called with the system lock held from ISR context.
 *
 * @param[in] i2cp      pointer to the @p hal_i2c_driver_c object
 */
static void i2c_lld_transmit_data(hal_i2c_driver_c *i2cp) {
  I2C_TypeDef *dp = i2cp->i2c;
  uint32_t data;

  while ((i2cp->txbytes > 0U) &&
         ((dp->STATUS & I2C_IC_STATUS_TFNF) != 0U)) {
    data = (uint32_t)*i2cp->txptr;

    /* Send STOP after the last byte of a pure write. If a read phase
       follows, the transfer continues with a repeated START instead.*/
    if ((i2cp->txbytes == 1U) && (i2cp->rxbytes == 0U)) {
      data |= I2C_IC_DATA_CMD_STOP;
      i2cp->stop_expected = true;
    }
    dp->DATACMD = data;

    i2cp->txptr++;
    i2cp->txbytes--;
  }

  if (i2cp->txbytes == 0U) {
    if (i2cp->rxbytes > 0U) {
      /* All write commands are queued and a read phase follows. Switch
         to the RX state now, the first read command will carry the
         RESTART flag producing a repeated START on the wire. The
         TX_EMPTY interrupt is kept enabled, with TX_EMPTY_CTRL set it
         fires again only when the last write byte has completed on the
         wire and the service then queues the read commands via
         i2c_lld_request_data().*/
      i2cp->state = I2C_ACTIVE_RX;
      i2cp->send_restart = true;
    }
    else {
      /* Nothing more to send, disable TX FIFO empty interrupt.*/
      dp->CLR.INTRMASK = I2C_IC_INTR_MASK_M_TX_EMPTY;
    }
  }
}

/**
 * @brief   Drains the receive FIFO into the transfer buffer.
 * @note    Called with the system lock held while the transfer
 *          generation is open, the buffer belongs to the driver only
 *          until the terminal event is claimed.
 *
 * @param[in] i2cp      pointer to the @p hal_i2c_driver_c object
 */
static void i2c_lld_drain_rx(hal_i2c_driver_c *i2cp) {
  I2C_TypeDef *dp = i2cp->i2c;

  while ((dp->STATUS & I2C_IC_STATUS_RFNE) != 0U) {
    /* Read out received data.*/
    *i2cp->rxptr = (uint8_t)dp->DATACMD;
    i2cp->rxptr++;
  }
}

/**
 * @brief   Evaluates the own transfer completion evidence.
 * @details The own transfer has completed on the wire when all of the
 *          following holds, evaluated on live hardware state:
 *          - The command carrying the STOP bit has been queued. Foreign
 *            STOP conditions can only be observed before this point or
 *            while own commands are still queued or executing, once the
 *            controller owns the bus every STOP is its own.
 *          - The TX FIFO has fully drained, no own command is waiting
 *            for the bus.
 *          - The raw TX_EMPTY flag is set. With TX_EMPTY_CTRL selected
 *            this flag is completion qualified: it only rises after the
 *            command popped last, here the STOP carrying one, has
 *            completed on the wire.
 *          For an open generation the evidence is stable once true, it
 *          is only invalidated by the next transfer start.
 * @note    Called with the system lock held.
 *
 * @param[in] i2cp      pointer to the @p hal_i2c_driver_c object
 * @return              The completion evidence state.
 */
static bool i2c_lld_transfer_ended(hal_i2c_driver_c *i2cp) {
  I2C_TypeDef *dp = i2cp->i2c;

  return i2cp->stop_expected &&
         (dp->TXFLR == 0U) &&
         ((dp->RAWINTRSTAT & I2C_IC_INTR_STAT_R_TX_EMPTY) != 0U);
}

/**
 * @brief   Claims the terminal event and terminates the transfer.
 * @details This is the single point where an interrupt service wins the
 *          terminal event of an open transfer generation. The claim,
 *          the peripheral quiescing, the driver state transition and
 *          the waiter resumption form one atomic unit inside the
 *          caller's critical section. This replicates the I-class core
 *          of the shared @p __i2c_complete_isr() and @p __i2c_error_isr()
 *          helpers which cannot be used here: they open a critical
 *          section of their own after the claim section has closed,
 *          leaving a window in which @p i2cStopTransferI() on the other
 *          core observes a still active driver state on an already
 *          claimed generation and wakes the waiter for a transfer that
 *          has terminated, unconditionally, its wakeup is not gated on
 *          the low level stop result. The user callback, the only piece
 *          which must run outside the critical section, remains with
 *          the caller.
 * @note    Must be kept semantically aligned with the shared helpers in
 *          hal_i2c.h: same terminal driver state, same wakeup messages,
 *          waiter resumption only with the synchronization API enabled.
 * @note    Called with the system lock held.
 *
 * @param[in] i2cp      pointer to the @p hal_i2c_driver_c object
 * @param[in] msg       the wakeup message
 */
static void i2c_lld_claim_terminal(hal_i2c_driver_c *i2cp, msg_t msg) {
  I2C_TypeDef *dp = i2cp->i2c;

  chDbgAssert((i2cp->tgen & 1U) != 0U, "generation already claimed");

  /* Terminal event claimed, the generation counter becomes even, every
     other terminal path loses from here.*/
  i2cp->tgen++;

  /* Peripheral quiesced within the claim critical section, no start
     can interleave because starts run under the same lock.*/
  dp->INTRMASK = 0U;
  (void)dp->CLRINTR;

  /* Driver state transition and waiter resumption, atomic with the
     claim above.*/
  i2cp->state = HAL_DRV_STATE_READY;
#if I2C_USE_SYNCHRONIZATION == TRUE
  chThdResumeI(&i2cp->sync_transfer, msg);
#else
  (void)msg;
#endif
}

/**
 * @brief   Transmission errors service.
 * @details The terminal event is claimed against the transfer generation
 *          counter and the raw interrupt state, the error decoding and
 *          the whole transfer termination are performed in one critical
 *          section, see @p i2c_lld_claim_terminal(). An error made stale
 *          by a concurrent stop or already consumed by a new transfer
 *          start loses the claim and is silenced without side effects.
 *          The abort completion following a transfer stop takes the
 *          losing path: the stop already claimed the terminal event,
 *          the service only clears the flags and releases the abort
 *          tracking.
 *
 * @param[in] i2cp      pointer to the @p hal_i2c_driver_c object
 */
static void i2c_lld_serve_errors(hal_i2c_driver_c *i2cp) {
  I2C_TypeDef *dp = i2cp->i2c;
  uint32_t raw, abort_source;
  bool claimed;

  chSysLockFromISR();
  raw = dp->RAWINTRSTAT;

  /* A raw abort event doubles as the completion notification of the
     asynchronous abort initiated by a transfer stop, the hardware
     raises it only after the ABORT request has finished flushing.*/
  if ((raw & I2C_IC_INTR_STAT_R_TX_ABRT) != 0U) {
    i2cp->abort_pending = false;
  }

  claimed = ((i2cp->tgen & 1U) != 0U) &&
            ((raw & (I2C_IC_INTR_STAT_R_TX_ABRT | I2C_OVERRUN_ERRORS)) != 0U);
  if (claimed) {
    /* Abort cause decoding, the source register is cleared together
       with the interrupt flags by the claim below.*/
    abort_source = dp->TXABRTSOURCE;
    if ((abort_source & I2C_IC_TX_ABRT_SOURCE_ARB_LOST) != 0U) {
      i2cp->errors |= I2C_ARBITRATION_LOST;
    }
    if ((abort_source & (I2C_IC_TX_ABRT_SOURCE_ABRT_7B_ADDR_NOACK |
                         I2C_IC_TX_ABRT_SOURCE_ABRT_10ADDR1_NOACK |
                         I2C_IC_TX_ABRT_SOURCE_ABRT_10ADDR2_NOACK |
                         I2C_IC_TX_ABRT_SOURCE_ABRT_GCALL_NOACK   |
                         I2C_IC_TX_ABRT_SOURCE_ABRT_TXDATA_NOACK)) != 0U) {
      i2cp->errors |= I2C_ACK_FAILURE;
    }
    if ((raw & I2C_OVERRUN_ERRORS) != 0U) {
      i2cp->errors |= I2C_OVERRUN;
    }

    /* Claim, quiescing, driver state transition and waiter resumption,
       one atomic unit.*/
    i2c_lld_claim_terminal(i2cp, MSG_RESET);
  }
  else {
    if ((i2cp->tgen & 1U) == 0U) {
      /* No transfer in flight, the flags belong to a terminated or
         stopped transfer, silenced without dispatching.*/
      dp->INTRMASK = 0U;
      (void)dp->CLRINTR;
    }
    /* With a transfer in flight and no raw error left the flags have
       already been consumed by a transfer start, nothing to do.*/
  }
  chSysUnlockFromISR();

  if (claimed) {
    /* The user callback is the only piece running outside the critical
       section, invoked by the single terminal-event winner.*/
    __cbdrv_invoke_cb(i2cp);
  }
}

/**
 * @brief   TX FIFO empty service.
 * @details Feeds the transmit FIFO or queues read commands. The whole
 *          service runs in a critical section: the generation check and
 *          the FIFO accesses must be atomic with respect to a transfer
 *          stop on the other core, otherwise the engine could feed an
 *          aborting transfer or unmask interrupts behind its back.
 *
 * @param[in] i2cp      pointer to the @p hal_i2c_driver_c object
 */
static void i2c_lld_serve_tx_empty(hal_i2c_driver_c *i2cp) {

  chSysLockFromISR();
  if ((i2cp->tgen & 1U) != 0U) {
    if (i2cp->state == HAL_DRV_STATE_ACTIVE) {
      i2c_lld_transmit_data(i2cp);
    }
    else {
      i2c_lld_request_data(i2cp);
    }
  }
  else {
    /* No transfer in flight, a stale data interrupt is silenced. The
       mask write cannot stomp a concurrent start because starts run
       under the same lock.*/
    i2cp->i2c->CLR.INTRMASK = I2C_IC_INTR_MASK_M_TX_EMPTY |
                              I2C_IC_INTR_MASK_M_RX_FULL;
  }
  chSysUnlockFromISR();
}

/**
 * @brief   RX FIFO batch service.
 * @details Drains the receive FIFO into the transfer buffer. The whole
 *          service runs in a critical section: after a transfer stop
 *          wins the terminal event the caller owns the receive buffer
 *          again, draining into it is only legal while the generation
 *          is still open.
 *
 * @param[in] i2cp      pointer to the @p hal_i2c_driver_c object
 */
static void i2c_lld_serve_rx_full(hal_i2c_driver_c *i2cp) {
  I2C_TypeDef *dp = i2cp->i2c;

  chSysLockFromISR();
  if ((i2cp->tgen & 1U) != 0U) {
    i2c_lld_drain_rx(i2cp);

    if (i2cp->rxbytes == 0U) {
      /* Everything is received, therefore disable all FIFO IRQs.*/
      dp->CLR.INTRMASK = I2C_IC_INTR_MASK_M_RX_FULL |
                         I2C_IC_INTR_MASK_M_TX_EMPTY;
    }
    else {
      /* Enable TX FIFO empty IRQ to request more data.*/
      dp->SET.INTRMASK = I2C_IC_INTR_MASK_M_TX_EMPTY;
    }
  }
  else {
    /* No transfer in flight, the buffer is not touched, residual data
       is flushed by the enable cycle of the next transfer start.*/
    dp->CLR.INTRMASK = I2C_IC_INTR_MASK_M_RX_FULL |
                       I2C_IC_INTR_MASK_M_TX_EMPTY;
  }
  chSysUnlockFromISR();
}

/**
 * @brief   STOP detection service.
 * @details Completion arbitration. The RP silicon hardwires the
 *          IC_CON.STOP_DET_IF_MASTER_ACTIVE feature off (the write is
 *          inert, verified by register read-back), the STOP_DET flag
 *          therefore also latches STOP conditions issued by other
 *          masters and, being a single flag, coalesces multiple STOP
 *          events. Completion is consequently decided only on the
 *          own-completion evidence evaluated by
 *          @p i2c_lld_transfer_ended() on live hardware state, never on
 *          the flag itself. A foreign STOP must still be consumed or
 *          the level-triggered interrupt storms, but the engine can
 *          complete the own transfer between any evaluation and the
 *          clear, the coalescing flag would then swallow the own STOP
 *          event. The evidence is therefore reevaluated after every
 *          clear within the same critical section: evidence appearing
 *          after the clear is the swallowed own STOP and is claimed as
 *          the completion, an own STOP arriving after the reevaluation
 *          latches the flag again and redispatches this service,
 *          nothing is lost. With the evidence positive the final read
 *          data, if any, is already complete in the RX FIFO, it is
 *          drained here while the generation is still open.
 *
 * @param[in] i2cp      pointer to the @p hal_i2c_driver_c object
 */
static void i2c_lld_serve_stop(hal_i2c_driver_c *i2cp) {
  I2C_TypeDef *dp = i2cp->i2c;
  bool claimed = false;

  chSysLockFromISR();
  if (((i2cp->tgen & 1U) != 0U) &&
      ((dp->RAWINTRSTAT & (I2C_IC_INTR_STAT_R_TX_ABRT |
                           I2C_OVERRUN_ERRORS)) == 0U)) {
    if (!i2c_lld_transfer_ended(i2cp)) {
      /* Foreign STOP so far, consumed and immediately reevaluated, see
         the details above.*/
      (void)dp->CLRSTOPDET;
    }

    if (i2c_lld_transfer_ended(i2cp)) {
      /* Own STOP, the receive buffer is completed while the generation
         is still open, then the transfer is terminated.*/
      chDbgAssert((i2cp->txbytes == 0U) && (i2cp->rxbytes == 0U),
                  "counters not drained");

      i2c_lld_drain_rx(i2cp);
      i2c_lld_claim_terminal(i2cp, MSG_OK);
      claimed = true;
    }
  }
  else {
    /* Stale STOP with no transfer in flight, or a STOP trailing an
       abort or overrun which is terminal through the error service:
       dropped without touching the rest of the peripheral state.*/
    (void)dp->CLRSTOPDET;
  }
  chSysUnlockFromISR();

  if (claimed) {
    /* The user callback is the only piece running outside the critical
       section, invoked by the single terminal-event winner.*/
    __cbdrv_invoke_cb(i2cp);
  }
}

/**
 * @brief   I2C shared ISR code.
 *
 * @param[in] i2cp      pointer to the @p hal_i2c_driver_c object
 */
static void i2c_lld_serve_interrupt(hal_i2c_driver_c *i2cp) {
  I2C_TypeDef *dp = i2cp->i2c;
  uint32_t intr = dp->INTRSTAT;

  /* Transmission error detected, includes the abort completion of a
     stopped transfer.*/
  if ((intr & I2C_ERROR_INTERRUPTS) != 0U) {
    i2c_lld_serve_errors(i2cp);
    return;
  }

  /* If the TX FIFO is empty we can request or send more data.*/
  if ((intr & I2C_IC_INTR_STAT_R_TX_EMPTY) != 0U) {
    i2c_lld_serve_tx_empty(i2cp);
    return;
  }

  /* A batch of data has been received.*/
  if ((intr & I2C_IC_INTR_STAT_R_RX_FULL) != 0U) {
    i2c_lld_serve_rx_full(i2cp);
    return;
  }

  /* STOP condition detected, own STOPs terminate the transfer.*/
  if ((intr & I2C_IC_INTR_STAT_R_STOP_DET) != 0U) {
    i2c_lld_serve_stop(i2cp);
  }
}

#if (HAL_USE_PAL == TRUE) || defined(__DOXYGEN__)
/**
 * @brief   Waits for a line to read high.
 * @details The line is sampled at the bus clear half period pace, the
 *          wait is bounded, see @p I2C_BUS_CLEAR_SCL_RISE_CYCLES.
 * @note    Thread context only, this function sleeps between samples.
 *
 * @param[in] line      line identifier
 * @return              The line state.
 * @retval true         if the line reads high within the bound.
 * @retval false        if the line never rose.
 */
static bool i2c_lld_wait_line_high(ioline_t line) {
  unsigned i;

  for (i = 0U; i < I2C_BUS_CLEAR_SCL_RISE_CYCLES; i++) {
    if (palReadLine(line) == PAL_HIGH) {
      return true;
    }

    chThdSleep(TIME_US2I(I2C_BUS_CLEAR_HALF_PERIOD_US));
  }

  return false;
}
#endif /* HAL_USE_PAL == TRUE */

/*===========================================================================*/
/* Driver interrupt handlers.                                                */
/*===========================================================================*/

#if (RP_I2C_USE_I2C0 == TRUE) || defined(__DOXYGEN__)
/**
 * @brief   I2C0 interrupt handler.
 *
 * @isr
 */
CH_IRQ_HANDLER(RP_I2C0_IRQ_HANDLER) {

  CH_IRQ_PROLOGUE();

  i2c_lld_serve_interrupt(&I2CD0);

  CH_IRQ_EPILOGUE();
}
#endif

#if (RP_I2C_USE_I2C1 == TRUE) || defined(__DOXYGEN__)
/**
 * @brief   I2C1 interrupt handler.
 *
 * @isr
 */
CH_IRQ_HANDLER(RP_I2C1_IRQ_HANDLER) {

  CH_IRQ_PROLOGUE();

  i2c_lld_serve_interrupt(&I2CD1);

  CH_IRQ_EPILOGUE();
}
#endif

/*===========================================================================*/
/* Driver exported functions.                                                */
/*===========================================================================*/

/**
 * @brief   Low level I2C driver initialization.
 *
 * @notapi
 */
void i2c_lld_init(void) {

#if RP_I2C_USE_I2C0 == TRUE
  i2cObjectInit(&I2CD0);
  I2CD0.i2c           = I2C0;
  I2CD0.tgen          = 0U;
  I2CD0.stop_expected = false;
  I2CD0.abort_pending = false;

  /* Reset I2C.*/
  rp_peripheral_reset(RESETS_ALLREG_I2C0);
#endif

#if RP_I2C_USE_I2C1 == TRUE
  i2cObjectInit(&I2CD1);
  I2CD1.i2c           = I2C1;
  I2CD1.tgen          = 0U;
  I2CD1.stop_expected = false;
  I2CD1.abort_pending = false;

  /* Reset I2C.*/
  rp_peripheral_reset(RESETS_ALLREG_I2C1);
#endif
}

/**
 * @brief   Configures and activates the I2C peripheral.
 *
 * @param[in] i2cp      pointer to the @p hal_i2c_driver_c object
 * @return              The operation status.
 *
 * @notapi
 */
msg_t i2c_lld_start(hal_i2c_driver_c *i2cp) {

  /* Resources claim and peripheral activation.*/
  if (false) {
  }
#if RP_I2C_USE_I2C0 == TRUE
  else if (&I2CD0 == i2cp) {
    rp_peripheral_unreset(RESETS_ALLREG_I2C0);
    nvicEnableVector(RP_I2C0_IRQ_NUMBER, RP_IRQ_I2C0_PRIORITY);
  }
#endif
#if RP_I2C_USE_I2C1 == TRUE
  else if (&I2CD1 == i2cp) {
    rp_peripheral_unreset(RESETS_ALLREG_I2C1);
    nvicEnableVector(RP_I2C1_IRQ_NUMBER, RP_IRQ_I2C1_PRIORITY);
  }
#endif
  else {
    chDbgAssert(false, "invalid I2C instance");
    return HAL_RET_NO_RESOURCE;
  }

  /* Register programming is delegated to the configuration method, a
     NULL configuration selects the driver default.*/
  i2cp->config = i2c_lld_setcfg(i2cp, (const hal_i2c_config_t *)i2cp->config);
  if (i2cp->config == NULL) {
    /* A rejected configuration must not leave the peripheral active,
       the activation performed above is undone so that the shared
       driver returns to the stop state cleanly.*/
    i2c_lld_deactivate(i2cp);

    return HAL_RET_CONFIG_ERROR;
  }

  return HAL_RET_SUCCESS;
}

/**
 * @brief   Deactivates the I2C peripheral.
 *
 * @param[in] i2cp      pointer to the @p hal_i2c_driver_c object
 *
 * @notapi
 */
void i2c_lld_stop(hal_i2c_driver_c *i2cp) {

  if (i2cp->state != HAL_DRV_STATE_STOP) {
    /* Any late terminal event loses its claim before the peripheral
       goes away, interrupt sources are silenced under the same lock.
       The peripheral reset below also cancels a still flushing
       asynchronous abort, the software tracking is released with it.*/
    chSysLock();
    if ((i2cp->tgen & 1U) != 0U) {
      i2cp->tgen++;
    }
    i2cp->abort_pending = false;
    i2cp->i2c->INTRMASK = 0U;
    chSysUnlock();

    i2c_lld_deactivate(i2cp);
  }
}

/**
 * @brief   I2C configuration.
 * @details The SCL counts and the spike suppression length are derived
 *          from the requested rate and the current system clock using
 *          the classic timing ratios, configurations violating the
 *          field ranges or the databook relations between the counts
 *          and the suppression length are rejected. The peripheral is
 *          reprogrammed only while no transfer is in flight and no
 *          abort is still flushing: the controller disable performed
 *          for reprogramming then settles within the documented two
 *          clock cycles, the bounded handshake is a formality.
 *
 * @param[in] i2cp      pointer to the @p hal_i2c_driver_c object
 * @param[in] config    pointer to the @p hal_i2c_config_t structure
 * @return              A pointer to the current configuration structure.
 * @retval NULL         if the configuration failed.
 *
 * @notapi
 */
const hal_i2c_config_t *i2c_lld_setcfg(hal_i2c_driver_c *i2cp,
                                       const hal_i2c_config_t *config) {
  I2C_TypeDef *dp = i2cp->i2c;
  uint32_t freq_in, baudrate, period, lcnt, hcnt, spklen, sda_tx_hold;

  if (config == NULL) {
    config = &i2c_default_config;
  }

  /* A configuration change is only legal while no transfer is in
     flight and no asynchronous abort is still flushing.*/
  if (((i2cp->tgen & 1U) != 0U) || i2cp->abort_pending) {
    return NULL;
  }

  /* Timing derivation, the classic math with rejection instead of
     assertion.*/
  freq_in = (uint32_t)halClockGetPointX(RP_CLK_SYS);
  baudrate = config->baudrate;

  /* The RP silicon supports up to Fast Mode Plus.*/
  if ((baudrate == 0U) || (baudrate > 1000000U)) {
    return NULL;
  }

  period = (freq_in + (baudrate / 2U)) / baudrate;

  /* Periods too long for the 16-bit count fields are rejected early,
     this also keeps the ratio scaling below within 32 bits.*/
  if (period > 0x20000U) {
    return NULL;
  }

  if (baudrate <= 100000U) {
    /* Standard Mode: H: 4000ns, L: 4700ns ~ 0.54.*/
    lcnt = period * 54U / 100U;
  }
  else if (baudrate <= 400000U) {
    /* Fast Mode: H: 600ns, L: 1300ns ~ 0.68.*/
    lcnt = period * 68U / 100U;
  }
  else {
    /* Fast Mode Plus: H: 500ns, L: 760ns ~ 0.60.*/
    lcnt = period * 60U / 100U;
  }
  hcnt = period - lcnt;

  if ((lcnt > I2C_IC_FS_SCL_LCNT) || (hcnt > I2C_IC_FS_SCL_HCNT)) {
    return NULL;
  }

  /* Spike suppression length derived as one sixteenth of the low
     count, mirroring the reference implementation. Values beyond the
     8-bit field are clamped: a longer than nominal suppression window
     is harmless at the correspondingly slow SCL rates while silent
     truncation would program a meaningless length.*/
  spklen = lcnt < 16U ? 1U : lcnt / 16U;
  if (spklen > I2C_IC_FS_SPKLEN_IC_FS_SPKLEN) {
    spklen = I2C_IC_FS_SPKLEN_IC_FS_SPKLEN;
  }

  /* Databook validity relations between the SCL counts and the spike
     suppression length: LCNT must exceed SPKLEN + 7 and HCNT must
     exceed SPKLEN + 5.*/
  if ((lcnt <= (spklen + 7U)) || (hcnt <= (spklen + 5U))) {
    return NULL;
  }

  if (baudrate < 1000000U) {
    /* Standard and Fast Mode:
       sda_tx_hold = freq_in [cycles/s] * 300ns * (1s / 1e9ns)
       Reduce 300/1e9 to 3/1e7 to avoid numbers that don't fit in u32.
       Add 1 to avoid division truncation.*/
    sda_tx_hold = ((freq_in * 3U) / 10000000U) + 1U;
  }
  else {
    /* Fast Mode Plus:
       sda_tx_hold = freq_in [cycles/s] * 120ns * (1s / 1e9ns)
       Reduce 120/1e9 to 3/25e6 to avoid numbers that don't fit in u32.
       Add 1 to avoid division truncation.*/
    sda_tx_hold = ((freq_in * 3U) / 25000000U) + 1U;
  }

  if (sda_tx_hold > (lcnt - 2U)) {
    return NULL;
  }

  /* With the peripheral still held in reset only the validation is
     performed: the shared start sequence applies a new configuration
     before activating the peripheral, the register programming below
     is then performed by i2c_lld_start() which invokes this method
     again after releasing the reset.*/
  if ((RESETS->RESET_DONE & i2c_lld_reset_mask(i2cp)) == 0U) {
    return config;
  }

  /* Controller disabled during reprogramming, the disable is confirmed
     through the ENABLE_STATUS handshake before touching the enable
     gated registers.*/
  dp->ENABLE = 0U;
  if (!i2c_lld_wait_enable_state(dp, 0U)) {
    return NULL;
  }

  dp->CON = I2C_IC_CON_IC_SLAVE_DISABLE |
            I2C_IC_CON_IC_RESTART_EN |
#if RP_I2C_ADDRESS_MODE_10BIT == TRUE
            I2C_IC_CON_IC_10BITADDR_MASTER |
#endif
            (2U << I2C_IC_CON_SPEED_Pos) | /* Always Fast Mode.*/
            I2C_IC_CON_MASTER_MODE |
            I2C_IC_CON_STOP_DET_IF_MASTER_ACTIVE |
            I2C_IC_CON_TX_EMPTY_CTRL;

  dp->RXTL = 0U;
  dp->TXTL = 0U;

  /* All timing values validated above, written as derived.*/
  dp->FSSCLHCNT = hcnt;
  dp->FSSCLLCNT = lcnt;
  dp->FSSPKLEN = spklen;
  dp->SDAHOLD = sda_tx_hold;

  /* Interrupt sources masked until a transfer starts.*/
  dp->INTRMASK = 0U;

  /* Peripheral enabled again with the enable confirmed, flags
     cleared.*/
  dp->ENABLE = I2C_IC_ENABLE_ENABLE;
  if (!i2c_lld_wait_enable_state(dp, I2C_IC_ENABLE_STATUS_IC_EN)) {
    return NULL;
  }
  (void)dp->CLRINTR;

  return config;
}

/**
 * @brief   Selects one of the pre-defined I2C configurations.
 *
 * @param[in] i2cp      pointer to the @p hal_i2c_driver_c object
 * @param[in] cfgnum    driver configuration number
 * @return              The configuration pointer.
 *
 * @notapi
 */
const hal_i2c_config_t *i2c_lld_selcfg(hal_i2c_driver_c *i2cp,
                                       unsigned cfgnum) {
#if I2C_USE_CONFIGURATIONS == TRUE
  extern const i2c_configurations_t i2c_configurations;

  if (cfgnum >= i2c_configurations.cfgsnum) {
    return NULL;
  }

  return i2c_lld_setcfg(i2cp, &i2c_configurations.cfgs[cfgnum]);
#else

  if (cfgnum > 0U) {
    return NULL;
  }

  return i2c_lld_setcfg(i2cp, NULL);
#endif
}

/**
 * @brief   Low level callback configuration hook.
 *
 * @param[in] i2cp      pointer to the @p hal_i2c_driver_c object
 * @param[in] cb        callback pointer
 *
 * @notapi
 */
void i2c_lld_set_callback(hal_i2c_driver_c *i2cp, drv_cb_t cb) {

  (void)i2cp;
  (void)cb;
}

/**
 * @brief   Stops the ongoing I2C transfer.
 * @details The terminal event is claimed synchronously, this function is
 *          invoked with the system lock held so the claim is atomic with
 *          respect to the completion and error services on either core.
 *          Those services terminate the driver state inside their own
 *          claim critical section, a driver state still reported as
 *          active to the caller therefore implies an unclaimed
 *          generation: this claim then always wins and the shared layer
 *          performs the state transition and the waiter wakeup under
 *          the same lock it already holds. The hardware abort is only
 *          initiated here and tracked by the abort-pending flag, its
 *          completion arrives later as the TX_ABRT interrupt which
 *          finds the generation already even, clears the flags and
 *          releases the tracking. Transfer starts are rejected while
 *          the abort is pending, see @p i2c_lld_check_startable().
 * @note    If a slave holds SCL low indefinitely the abort cannot
 *          complete and the controller remains busy, recovery requires
 *          @p i2cRPBusClear() followed by a driver restart.
 *
 * @param[in] i2cp      pointer to the @p hal_i2c_driver_c object
 * @return              The operation status.
 *
 * @notapi
 */
msg_t i2c_lld_stop_transfer(hal_i2c_driver_c *i2cp) {
  I2C_TypeDef *dp = i2cp->i2c;

  if ((i2cp->tgen & 1U) != 0U) {
    i2cp->tgen++;

    /* Data and completion interrupt sources silenced, only the abort
       completion event is left enabled.*/
    dp->INTRMASK = I2C_IC_INTR_MASK_M_TX_ABRT;

    /* Abort initiation, the request self-clears when the hardware has
       flushed the TX FIFO and terminated the transfer on the wire.
       Tracked in software because the ACTIVITY status alone does not
       cover the whole flush.*/
    i2cp->abort_pending = true;
    dp->ENABLE |= I2C_IC_ENABLE_ABORT;
  }

  return HAL_RET_SUCCESS;
}

/**
 * @brief   Receives data via the I2C bus as master.
 * @details This asynchronous function starts a receive operation, the
 *          transfer is advanced entirely by the interrupt services and
 *          terminates with the wakeup/callback machinery.
 *
 * @param[in] i2cp      pointer to the @p hal_i2c_driver_c object
 * @param[in] addr      slave device address
 * @param[out] rxbuf    pointer to the receive buffer
 * @param[in] rxbytes   number of bytes to be received
 * @return              The operation status.
 * @retval HAL_RET_SUCCESS     if the function succeeded.
 * @retval HAL_RET_HW_BUSY     if the controller is busy or an abort is
 *                             still flushing, no waiting is performed,
 *                             the caller retries or fails the
 *                             operation.
 *
 * @notapi
 */
msg_t i2c_lld_start_master_receive(hal_i2c_driver_c *i2cp, i2caddr_t addr,
                                   uint8_t *rxbuf, size_t rxbytes) {
  msg_t msg;

  /* Start gate, a busy controller or a still flushing abort rejects
     the start, this is an I-class context and waiting is not
     allowed.*/
  msg = i2c_lld_check_startable(i2cp);
  if (msg != HAL_RET_SUCCESS) {
    return msg;
  }

  /* Transaction setup.*/
  i2cp->txptr         = NULL;
  i2cp->txbytes       = (size_t)0;
  i2cp->rxptr         = rxbuf;
  i2cp->rxbytes       = rxbytes;
  i2cp->send_restart  = false;
  i2cp->stop_expected = false;

  /* Hardware programming and start, a failed enable handshake rejects
     the start before the transfer generation is opened.*/
  msg = i2c_lld_setup_transfer(i2cp, addr);
  if (msg != HAL_RET_SUCCESS) {
    return msg;
  }

  /* Transfer generation opened, from here the terminal event belongs
     to a single winner among the completion, error and stop paths.*/
  i2c_lld_tgen_open(i2cp);

  return HAL_RET_SUCCESS;
}

/**
 * @brief   Transmits data via the I2C bus as master.
 * @details This asynchronous function starts a transmit operation, when
 *          @p rxbytes is greater than zero a receive phase follows the
 *          transmit phase using a repeated START. The transfer is
 *          advanced entirely by the interrupt services and terminates
 *          with the wakeup/callback machinery.
 *
 * @param[in] i2cp      pointer to the @p hal_i2c_driver_c object
 * @param[in] addr      slave device address
 * @param[in] txbuf     pointer to the transmit buffer
 * @param[in] txbytes   number of bytes to be transmitted
 * @param[out] rxbuf    pointer to the receive buffer
 * @param[in] rxbytes   number of bytes to be received
 * @return              The operation status.
 * @retval HAL_RET_SUCCESS     if the function succeeded.
 * @retval HAL_RET_HW_BUSY     if the controller is busy or an abort is
 *                             still flushing, no waiting is performed,
 *                             the caller retries or fails the
 *                             operation.
 *
 * @notapi
 */
msg_t i2c_lld_start_master_transmit(hal_i2c_driver_c *i2cp, i2caddr_t addr,
                                    const uint8_t *txbuf, size_t txbytes,
                                    uint8_t *rxbuf, size_t rxbytes) {
  msg_t msg;

  /* Start gate, a busy controller or a still flushing abort rejects
     the start, this is an I-class context and waiting is not
     allowed.*/
  msg = i2c_lld_check_startable(i2cp);
  if (msg != HAL_RET_SUCCESS) {
    return msg;
  }

  /* Transaction setup.*/
  i2cp->txptr         = txbuf;
  i2cp->txbytes       = txbytes;
  i2cp->rxptr         = rxbuf;
  i2cp->rxbytes       = rxbytes;
  i2cp->send_restart  = false;
  i2cp->stop_expected = false;

  /* Hardware programming and start, a failed enable handshake rejects
     the start before the transfer generation is opened.*/
  msg = i2c_lld_setup_transfer(i2cp, addr);
  if (msg != HAL_RET_SUCCESS) {
    return msg;
  }

  /* Transfer generation opened, from here the terminal event belongs
     to a single winner among the completion, error and stop paths.*/
  i2c_lld_tgen_open(i2cp);

  return HAL_RET_SUCCESS;
}

#if (HAL_USE_PAL == TRUE) || defined(__DOXYGEN__)
/**
 * @brief   Attempts to clear a stuck I2C bus by clocking SCL.
 * @details Standard bus clear procedure: up to nine SCL pulses are
 *          issued until the slave holding SDA low releases it, then a
 *          STOP condition is generated. The lines are driven through
 *          the SIO function emulating open drain outputs, the caller
 *          must reassign the pads to the I2C function afterwards and
 *          restart the driver. Every SCL release is verified with a
 *          bounded wait: a clock line that never reads high is held by
 *          another device and such a bus cannot be recovered from this
 *          side, the procedure then fails instead of reporting a false
 *          success derived from the SDA observation alone. The STOP
 *          condition is generated only from a state with both lines
 *          observed releasable.
 * @note    Thread context only, this function sleeps between line
 *          transitions. It is not wired into any I-class path.
 *
 * @param[in] sclline   line identifier of the SCL signal
 * @param[in] sdaline   line identifier of the SDA signal
 * @return              The bus state.
 * @retval true         if both lines read high after the procedure.
 * @retval false        if a line is still stuck low.
 *
 * @api
 */
bool i2cRPBusClear(ioline_t sclline, ioline_t sdaline) {
  unsigned i;

  /* Both lines released and observed through the SIO function.*/
  palSetLineMode(sdaline, PAL_MODE_INPUT_PULLUP);
  palSetLineMode(sclline, PAL_MODE_INPUT_PULLUP);
  chThdSleep(TIME_US2I(I2C_BUS_CLEAR_HALF_PERIOD_US));

  /* SCL must be releasable before pulsing, a clock line held low
     cannot be recovered from this side of the bus.*/
  if (!i2c_lld_wait_line_high(sclline)) {
    return false;
  }

  for (i = 0U; i < 9U; i++) {
    if (palReadLine(sdaline) == PAL_HIGH) {
      break;
    }

    /* One SCL pulse, low is actively driven, high is released to the
       pull-up and verified: a device may legally stretch the low phase
       within the wait bound, a line that never rises fails the
       procedure.*/
    palClearLine(sclline);
    palSetLineMode(sclline, PAL_MODE_OUTPUT_PUSHPULL);
    chThdSleep(TIME_US2I(I2C_BUS_CLEAR_HALF_PERIOD_US));
    palSetLineMode(sclline, PAL_MODE_INPUT_PULLUP);
    if (!i2c_lld_wait_line_high(sclline)) {
      return false;
    }

    /* High phase settling time before the next pulse or the SDA
       sampling.*/
    chThdSleep(TIME_US2I(I2C_BUS_CLEAR_HALF_PERIOD_US));
  }

  /* The STOP condition is only meaningful on a bus with both lines
     releasable, a still held SDA is a failure.*/
  if (palReadLine(sdaline) != PAL_HIGH) {
    return false;
  }

  /* STOP condition: SDA driven low and then released while SCL stays
     high.*/
  palClearLine(sdaline);
  palSetLineMode(sdaline, PAL_MODE_OUTPUT_PUSHPULL);
  chThdSleep(TIME_US2I(I2C_BUS_CLEAR_HALF_PERIOD_US));
  palSetLineMode(sdaline, PAL_MODE_INPUT_PULLUP);
  chThdSleep(TIME_US2I(I2C_BUS_CLEAR_HALF_PERIOD_US));

  /* Success only with both lines observed high at the end.*/
  return (palReadLine(sclline) == PAL_HIGH) &&
         (palReadLine(sdaline) == PAL_HIGH);
}
#endif /* HAL_USE_PAL == TRUE */

#endif /* HAL_USE_I2C == TRUE */

/** @} */
