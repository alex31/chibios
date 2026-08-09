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
 * @brief   Programs the target address and starts the transfer engine.
 * @details The target address register is gated on the ENABLE register
 *          bit, cycling the enable also flushes both FIFOs from any
 *          residue of a previously aborted transfer. The RP DW_apb_i2c
 *          applies the enable state within two clk_sys cycles, hidden
 *          behind the following register accesses, therefore no
 *          settling wait is required with the controller idle.
 * @note    Called with the system lock held. The FIFO engine runs
 *          entirely in the interrupt service: with TX_EMPTY_CTRL
 *          selected the first TX_EMPTY interrupt fires as soon as the
 *          mask is armed because the TX FIFO is empty and no command is
 *          in flight, the service then queues data or read commands.
 *
 * @param[in] i2cp      pointer to the @p hal_i2c_driver_c object
 * @param[in] addr      slave device address
 */
static void i2c_lld_setup_transfer(hal_i2c_driver_c *i2cp, i2caddr_t addr) {
  I2C_TypeDef *dp = i2cp->i2c;

  dp->ENABLE = 0U;
  dp->TAR = (uint32_t)addr & I2C_IC_TAR_IC_TAR;
  dp->INTRMASK = 0U;
  dp->ENABLE = I2C_IC_ENABLE_ENABLE;

  /* Stale flags cleared, this also releases the TX FIFO from the
     flushed state following an abort.*/
  (void)dp->CLRINTR;

  /* Interrupt sources armed, the transfer proceeds in the service
     routines from here.*/
  dp->INTRMASK = I2C_IC_INTR_MASK_M_STOP_DET |
                 I2C_IC_INTR_MASK_M_TX_EMPTY |
                 I2C_ERROR_INTERRUPTS;
}

/**
 * @brief   Requests data to be received, actual reception is done in the
 *          interrupt handler.
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
 * @brief   Transmission errors service.
 * @details The terminal event is claimed against the transfer generation
 *          counter and the raw interrupt state before any driver state
 *          transition, see the notes in the driver header. An error made
 *          stale by a concurrent stop or already consumed by a new
 *          transfer start loses the claim and is silenced without side
 *          effects. The abort completion following a transfer stop takes
 *          this same path: the stop already claimed the terminal event,
 *          the service only clears the flags.
 *
 * @param[in] i2cp      pointer to the @p hal_i2c_driver_c object
 */
static void i2c_lld_serve_errors(hal_i2c_driver_c *i2cp) {
  I2C_TypeDef *dp = i2cp->i2c;
  uint32_t raw, abort_source;
  bool claimed;

  chSysLockFromISR();
  raw = dp->RAWINTRSTAT;
  claimed = ((i2cp->tgen & 1U) != 0U) &&
            ((raw & (I2C_IC_INTR_STAT_R_TX_ABRT | I2C_OVERRUN_ERRORS)) != 0U);
  if (claimed) {
    i2cp->tgen++;

    /* Abort cause decoding, the source register is cleared together
       with the interrupt flags below.*/
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

    /* Quiescing the peripheral within the claim critical section, no
       start can interleave because starts run under the same lock.*/
    dp->INTRMASK = 0U;
    (void)dp->CLRINTR;
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
    /* The wakeup machinery is invoked outside the critical section by
       the single terminal-event winner, the callback runs outside any
       lock.*/
    __i2c_error_isr(i2cp);
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
    while ((dp->STATUS & I2C_IC_STATUS_RFNE) != 0U) {
      /* Read out received data.*/
      *i2cp->rxptr = (uint8_t)dp->DATACMD;
      i2cp->rxptr++;
    }

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
 * @details Completion arbitration: the terminal event is claimed against
 *          the transfer generation counter and the current hardware
 *          state, all evaluated in one critical section. The RP silicon
 *          hardwires IC_CON.STOP_DET_IF_MASTER_ACTIVE off (the write is
 *          inert, verified by register read-back), so this interrupt
 *          also fires for STOP conditions issued by other masters on the
 *          bus. It only means completion when this driver's own final
 *          command has completed on the wire: the byte counters cover
 *          bytes not yet queued, TXFLR covers commands (data and read
 *          requests alike) still sitting in the TX FIFO waiting for the
 *          bus, and the raw TX_EMPTY flag - completion-qualified by
 *          TX_EMPTY_CTRL - stays low while a command popped from the
 *          FIFO is still executing. An own STOP can only appear after
 *          all of that has drained, so any residue marks the STOP as
 *          foreign; the own transfer then continues under hardware
 *          arbitration.
 *
 * @param[in] i2cp      pointer to the @p hal_i2c_driver_c object
 */
static void i2c_lld_serve_stop(hal_i2c_driver_c *i2cp) {
  I2C_TypeDef *dp = i2cp->i2c;
  uint32_t raw;
  bool claimed = false;

  chSysLockFromISR();
  raw = dp->RAWINTRSTAT;
  if (((i2cp->tgen & 1U) != 0U) &&
      ((raw & I2C_IC_INTR_STAT_R_STOP_DET) != 0U) &&
      ((raw & (I2C_IC_INTR_STAT_R_TX_ABRT | I2C_OVERRUN_ERRORS)) == 0U) &&
      (i2cp->txbytes == 0U) && (i2cp->rxbytes == 0U) &&
      (dp->TXFLR == 0U) &&
      ((raw & I2C_IC_INTR_STAT_R_TX_EMPTY) != 0U)) {
    if (dp->RXFLR == 0U) {
      /* Own STOP with everything drained, the completion is claimed
         and the peripheral quiesced within the critical section.*/
      claimed = true;
      i2cp->tgen++;
      dp->INTRMASK = 0U;
      (void)dp->CLRINTR;
    }
    /* Otherwise this is an own STOP racing the final RX batch: the
       flag is left pending, the pending RX FULL service drains the
       FIFO first and the STOP is then reevaluated on the next
       dispatch of this level interrupt.*/
  }
  else {
    /* Foreign or stale STOP condition, or a STOP trailing an abort
       which is terminal through the error service: dropped without
       touching the rest of the peripheral state.*/
    (void)dp->CLRSTOPDET;
  }
  chSysUnlockFromISR();

  if (claimed) {
    /* The wakeup machinery is invoked outside the critical section by
       the single terminal-event winner, the callback runs outside any
       lock.*/
    __i2c_complete_isr(i2cp);
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
  I2CD0.i2c  = I2C0;
  I2CD0.tgen = 0U;

  /* Reset I2C.*/
  rp_peripheral_reset(RESETS_ALLREG_I2C0);
#endif

#if RP_I2C_USE_I2C1 == TRUE
  i2cObjectInit(&I2CD1);
  I2CD1.i2c  = I2C1;
  I2CD1.tgen = 0U;

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
       goes away, interrupt sources are silenced under the same lock.*/
    chSysLock();
    if ((i2cp->tgen & 1U) != 0U) {
      i2cp->tgen++;
    }
    i2cp->i2c->INTRMASK = 0U;
    chSysUnlock();

    i2c_lld_deactivate(i2cp);
  }
}

/**
 * @brief   I2C configuration.
 * @details The SCL counts are derived from the requested rate and the
 *          current system clock using the classic timing ratios,
 *          configurations the divider fields cannot honor are rejected.
 *          The peripheral is reprogrammed only while no transfer is in
 *          flight: the controller disable performed for reprogramming
 *          settles within two clk_sys cycles on an idle controller, the
 *          bounded status poll below is a formality.
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
  uint32_t freq_in, baudrate, period, lcnt, hcnt, sda_tx_hold;

  if (config == NULL) {
    config = &i2c_default_config;
  }

  /* A configuration change is only legal while no transfer is in
     flight.*/
  if ((i2cp->tgen & 1U) != 0U) {
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

  if ((lcnt < 8U) || (lcnt > I2C_IC_FS_SCL_LCNT) ||
      (hcnt < 8U) || (hcnt > I2C_IC_FS_SCL_HCNT)) {
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

  /* Controller disabled during reprogramming, with no transfer in
     flight this settles within two clk_sys cycles.*/
  dp->ENABLE = 0U;
  while ((dp->ENABLESTATUS & I2C_IC_ENABLE_STATUS_IC_EN) != 0U) {
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

  dp->FSSCLHCNT = hcnt & I2C_IC_FS_SCL_HCNT;
  dp->FSSCLLCNT = lcnt & I2C_IC_FS_SCL_LCNT;
  dp->FSSPKLEN = (lcnt < 16U ? 1U : lcnt / 16U) &
                 I2C_IC_FS_SPKLEN_IC_FS_SPKLEN;
  dp->SDAHOLD = sda_tx_hold & I2C_IC_SDA_HOLD_IC_SDA_TX_HOLD;

  /* Interrupt sources masked until a transfer starts.*/
  dp->INTRMASK = 0U;

  /* Peripheral enabled again, flags cleared.*/
  dp->ENABLE = I2C_IC_ENABLE_ENABLE;
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
 *          The hardware abort is only initiated, its completion arrives
 *          later as the TX_ABRT interrupt which finds the generation
 *          already even and silently clears the flags. Until the abort
 *          completes the ACTIVITY status keeps the busy gate in the
 *          transfer start methods closed.
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

    /* Abort initiation, the bit self-clears when the hardware has
       flushed the TX FIFO and terminated the transfer on the wire.*/
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
 * @retval HAL_RET_HW_BUSY     if the controller or the bus is busy, no
 *                             waiting is performed, the caller retries
 *                             or fails the operation.
 *
 * @notapi
 */
msg_t i2c_lld_start_master_receive(hal_i2c_driver_c *i2cp, i2caddr_t addr,
                                   uint8_t *rxbuf, size_t rxbytes) {
  I2C_TypeDef *dp = i2cp->i2c;

  /* The controller is busy while a previous transfer, an abort or
     another bus master is still active, this is an I-class context and
     waiting is not allowed.*/
  (void)dp->CLRACTIVITY;
  if ((dp->STATUS & I2C_IC_STATUS_ACTIVITY) != 0U) {
    return HAL_RET_HW_BUSY;
  }

  /* Transaction setup.*/
  i2cp->txptr        = NULL;
  i2cp->txbytes      = (size_t)0;
  i2cp->rxptr        = rxbuf;
  i2cp->rxbytes      = rxbytes;
  i2cp->send_restart = false;

  /* Transfer generation opened, from here the terminal event belongs
     to a single winner among the completion, error and stop paths.*/
  i2c_lld_tgen_open(i2cp);

  /* Hardware programming and start.*/
  i2c_lld_setup_transfer(i2cp, addr);

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
 * @retval HAL_RET_HW_BUSY     if the controller or the bus is busy, no
 *                             waiting is performed, the caller retries
 *                             or fails the operation.
 *
 * @notapi
 */
msg_t i2c_lld_start_master_transmit(hal_i2c_driver_c *i2cp, i2caddr_t addr,
                                    const uint8_t *txbuf, size_t txbytes,
                                    uint8_t *rxbuf, size_t rxbytes) {
  I2C_TypeDef *dp = i2cp->i2c;

  /* The controller is busy while a previous transfer, an abort or
     another bus master is still active, this is an I-class context and
     waiting is not allowed.*/
  (void)dp->CLRACTIVITY;
  if ((dp->STATUS & I2C_IC_STATUS_ACTIVITY) != 0U) {
    return HAL_RET_HW_BUSY;
  }

  /* Transaction setup.*/
  i2cp->txptr        = txbuf;
  i2cp->txbytes      = txbytes;
  i2cp->rxptr        = rxbuf;
  i2cp->rxbytes      = rxbytes;
  i2cp->send_restart = false;

  /* Transfer generation opened, from here the terminal event belongs
     to a single winner among the completion, error and stop paths.*/
  i2c_lld_tgen_open(i2cp);

  /* Hardware programming and start.*/
  i2c_lld_setup_transfer(i2cp, addr);

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
 *          restart the driver.
 * @note    Thread context only, this function sleeps between line
 *          transitions. It is not wired into any I-class path.
 *
 * @param[in] sclline   line identifier of the SCL signal
 * @param[in] sdaline   line identifier of the SDA signal
 * @return              The bus state.
 * @retval true         if SDA reads high after the procedure.
 * @retval false        if SDA is still stuck low.
 *
 * @api
 */
bool i2cRPBusClear(ioline_t sclline, ioline_t sdaline) {
  unsigned i;

  /* Both lines released and observed through the SIO function.*/
  palSetLineMode(sdaline, PAL_MODE_INPUT_PULLUP);
  palSetLineMode(sclline, PAL_MODE_INPUT_PULLUP);
  chThdSleep(TIME_US2I(I2C_BUS_CLEAR_HALF_PERIOD_US));

  for (i = 0U; i < 9U; i++) {
    if (palReadLine(sdaline) == PAL_HIGH) {
      break;
    }

    /* One SCL pulse, low is actively driven, high is released to the
       pull-up.*/
    palClearLine(sclline);
    palSetLineMode(sclline, PAL_MODE_OUTPUT_PUSHPULL);
    chThdSleep(TIME_US2I(I2C_BUS_CLEAR_HALF_PERIOD_US));
    palSetLineMode(sclline, PAL_MODE_INPUT_PULLUP);
    chThdSleep(TIME_US2I(I2C_BUS_CLEAR_HALF_PERIOD_US));
  }

  /* STOP condition: SDA released while SCL is high.*/
  palClearLine(sdaline);
  palSetLineMode(sdaline, PAL_MODE_OUTPUT_PUSHPULL);
  chThdSleep(TIME_US2I(I2C_BUS_CLEAR_HALF_PERIOD_US));
  palSetLineMode(sdaline, PAL_MODE_INPUT_PULLUP);
  chThdSleep(TIME_US2I(I2C_BUS_CLEAR_HALF_PERIOD_US));

  return palReadLine(sdaline) == PAL_HIGH;
}
#endif /* HAL_USE_PAL == TRUE */

#endif /* HAL_USE_I2C == TRUE */

/** @} */
