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
 * @file    sb/user/sbuservio.h
 * @brief   ARM SandBox user VIO API.
 * @details Wrappers of the Virtual I/O (VIO) calls for guests not using the
 *          XHAL VIO port, for example bare-metal guests or guests running a
 *          non-ChibiOS kernel. Do not use together with the XHAL VIO port,
 *          some names overlap with the XHAL API.
 * @note    Units, configurations and VRQ numbers are assigned by the host
 *          configuration, the guest must use the same numbering.
 * @note    Configuration information copied by the host in the @p cfgp
 *          buffers, VRQ flags, events, errors and modes use the host XHAL
 *          drivers definitions. Asynchronous operations signal completion
 *          on the VRQ assigned to the unit, the VRQ flags contain the bit
 *          corresponding to the host driver state (for example
 *          @p HAL_DRV_STATE_COMPLETE or @p HAL_DRV_STATE_ERROR).
 * @note    Unless specified otherwise, functions returning @p msg_t return
 *          @p HAL_RET_SUCCESS or an error code.
 *
 * @addtogroup ARM_SANDBOX_USER_VIO_API
 * @{
 */

#ifndef SBUSERVIO_H
#define SBUSERVIO_H

#include <stddef.h>

#include "sbuser.h"

/*===========================================================================*/
/* Module constants.                                                         */
/*===========================================================================*/

/*===========================================================================*/
/* Module pre-compile time settings.                                         */
/*===========================================================================*/

/*===========================================================================*/
/* Derived constants and error checks.                                       */
/*===========================================================================*/

/*===========================================================================*/
/* Module data structures and types.                                         */
/*===========================================================================*/

/*===========================================================================*/
/* Module macros.                                                            */
/*===========================================================================*/

/*===========================================================================*/
/* External declarations.                                                    */
/*===========================================================================*/

/*===========================================================================*/
/* Module inline functions.                                                  */
/*===========================================================================*/

/**
 * @name    VIO GPIO API
 * @note    The host ignores operations on missing units or not permitted
 *          by the unit, reads return zero in that case.
 * @{
 */
/**
 * @brief   Writes a GPIO group.
 *
 * @param[in] unit      VIO GPIO unit
 * @param[in] bits      value to be written
 */
static inline void gpioWrite(uint32_t unit, uint32_t bits) {

  __syscall2r(96, VIO_CALL(SB_VGPIO_WRITE, unit), bits);
}

/**
 * @brief   Sets bits of a GPIO group.
 *
 * @param[in] unit      VIO GPIO unit
 * @param[in] bits      bits to be set
 */
static inline void gpioSet(uint32_t unit, uint32_t bits) {

  __syscall2r(96, VIO_CALL(SB_VGPIO_SET, unit), bits);
}

/**
 * @brief   Clears bits of a GPIO group.
 *
 * @param[in] unit      VIO GPIO unit
 * @param[in] bits      bits to be cleared
 */
static inline void gpioClear(uint32_t unit, uint32_t bits) {

  __syscall2r(96, VIO_CALL(SB_VGPIO_CLEAR, unit), bits);
}

/**
 * @brief   Toggles bits of a GPIO group.
 *
 * @param[in] unit      VIO GPIO unit
 * @param[in] bits      bits to be toggled
 */
static inline void gpioToggle(uint32_t unit, uint32_t bits) {

  __syscall2r(96, VIO_CALL(SB_VGPIO_TOGGLE, unit), bits);
}

/**
 * @brief   Reads the output latch of a GPIO group.
 *
 * @param[in] unit      VIO GPIO unit
 * @return              The latch value.
 */
static inline uint32_t gpioReadLatch(uint32_t unit) {

  __syscall1r(96, VIO_CALL(SB_VGPIO_READLATCH, unit));
  return r0;
}

/**
 * @brief   Reads a GPIO group.
 *
 * @param[in] unit      VIO GPIO unit
 * @return              The group value.
 */
static inline uint32_t gpioRead(uint32_t unit) {

  __syscall1r(96, VIO_CALL(SB_VGPIO_READ, unit));
  return r0;
}

/**
 * @brief   Sets the mode of bits of a GPIO group.
 *
 * @param[in] unit      VIO GPIO unit
 * @param[in] mask      bits mask, relative to @p offset
 * @param[in] offset    offset within the group
 * @param[in] mode      host PAL mode
 */
static inline void gpioSetMode(uint32_t unit, uint32_t mask,
                               uint32_t offset, uint32_t mode) {

  __syscall4r(96, VIO_CALL(SB_VGPIO_SETMODE, unit), mask, offset, mode);
}
/** @} */

/**
 * @name    VIO UART API
 * @{
 */
/**
 * @brief   Starts a UART unit using its default configuration.
 * @details Received data, transmission and error events are signaled on
 *          the unit VRQ, the events are initially disabled.
 *
 * @param[in] unit      VIO UART unit
 * @param[out] cfgp     buffer receiving the configuration, can be @p NULL
 * @param[in] n         size of the buffer
 * @return              The operation status.
 */
static inline msg_t uartInit(uint32_t unit, void *cfgp, size_t n) {

  __syscall3r(225, VIO_CALL(SB_VUART_INIT, unit), n, cfgp);
  return (msg_t)r0;
}

/**
 * @brief   Stops a UART unit.
 *
 * @param[in] unit      VIO UART unit
 * @return              The operation status.
 */
static inline msg_t uartDeinit(uint32_t unit) {

  __syscall1r(225, VIO_CALL(SB_VUART_DEINIT, unit));
  return (msg_t)r0;
}

/**
 * @brief   Selects a UART configuration.
 *
 * @param[in] unit      VIO UART unit
 * @param[in] ncfg      configuration number
 * @param[out] cfgp     buffer receiving the configuration, can be @p NULL
 * @param[in] n         size of the buffer
 * @return              The operation status.
 */
static inline msg_t uartSelectCfg(uint32_t unit, uint32_t ncfg,
                                  void *cfgp, size_t n) {

  __syscall4r(97, VIO_CALL(SB_VUART_SELCFG, unit), ncfg, n, cfgp);
  return (msg_t)r0;
}

/**
 * @brief   Determines the state of the RX FIFO.
 *
 * @param[in] unit      VIO UART unit
 * @return              The RX FIFO state.
 * @retval false        if the RX FIFO contains data.
 * @retval true         if the RX FIFO is empty.
 */
static inline bool uartIsRXEmpty(uint32_t unit) {

  __syscall1r(97, VIO_CALL(SB_VUART_ISRXE, unit));
  return (bool)(r0 != 0U);
}

/**
 * @brief   Determines the activity state of the receiver.
 *
 * @param[in] unit      VIO UART unit
 * @return              The RX activity state.
 * @retval false        if RX is active.
 * @retval true         if RX is idle.
 */
static inline bool uartIsRXIdle(uint32_t unit) {

  __syscall1r(97, VIO_CALL(SB_VUART_ISRXI, unit));
  return (bool)(r0 != 0U);
}

/**
 * @brief   Determines if RX has pending errors to be read and cleared.
 *
 * @param[in] unit      VIO UART unit
 * @return              The RX error state.
 * @retval false        if RX has no errors.
 * @retval true         if RX has errors.
 */
static inline bool uartHasRXErrors(uint32_t unit) {

  __syscall1r(97, VIO_CALL(SB_VUART_HASERR, unit));
  return (bool)(r0 != 0U);
}

/**
 * @brief   Determines the state of the TX FIFO.
 *
 * @param[in] unit      VIO UART unit
 * @return              The TX FIFO state.
 * @retval false        if the TX FIFO has space.
 * @retval true         if the TX FIFO is full.
 */
static inline bool uartIsTXFull(uint32_t unit) {

  __syscall1r(97, VIO_CALL(SB_VUART_ISTXF, unit));
  return (bool)(r0 != 0U);
}

/**
 * @brief   Determines the transmission state.
 *
 * @param[in] unit      VIO UART unit
 * @return              The TX transmission state.
 * @retval false        if transmission is over.
 * @retval true         if transmission is ongoing.
 */
static inline bool uartIsTXOngoing(uint32_t unit) {

  __syscall1r(97, VIO_CALL(SB_VUART_ISTXO, unit));
  return (bool)(r0 != 0U);
}

/**
 * @brief   Reads data from the RX FIFO without blocking.
 *
 * @param[in] unit      VIO UART unit
 * @param[out] buffer   buffer for the received data
 * @param[in] n         maximum number of frames to read
 * @return              The number of received frames.
 */
static inline size_t uartRead(uint32_t unit, uint8_t *buffer, size_t n) {

  __syscall3r(97, VIO_CALL(SB_VUART_READ, unit), buffer, n);
  return (size_t)r0;
}

/**
 * @brief   Writes data into the TX FIFO without blocking.
 *
 * @param[in] unit      VIO UART unit
 * @param[in] buffer    buffer containing the data to be transmitted
 * @param[in] n         maximum number of frames to write
 * @return              The number of written frames.
 */
static inline size_t uartWrite(uint32_t unit, const uint8_t *buffer,
                               size_t n) {

  __syscall3r(97, VIO_CALL(SB_VUART_WRITE, unit), buffer, n);
  return (size_t)r0;
}

/**
 * @brief   Returns one frame from the RX FIFO.
 * @note    The RX FIFO must not be empty.
 *
 * @param[in] unit      VIO UART unit
 * @return              The frame from RX FIFO.
 */
static inline msg_t uartGet(uint32_t unit) {

  __syscall1r(97, VIO_CALL(SB_VUART_GET, unit));
  return (msg_t)r0;
}

/**
 * @brief   Pushes one frame into the TX FIFO.
 * @note    The TX FIFO must not be full.
 *
 * @param[in] unit      VIO UART unit
 * @param[in] data      frame to be written
 */
static inline void uartPut(uint32_t unit, uint32_t data) {

  __syscall2r(97, VIO_CALL(SB_VUART_PUT, unit), data);
}

/**
 * @brief   Writes the mask of events enabled to trigger the unit VRQ.
 *
 * @param[in] unit      VIO UART unit
 * @param[in] mask      host SIO events mask
 */
static inline void uartWriteEnableFlags(uint32_t unit, uint32_t mask) {

  __syscall2r(97, VIO_CALL(SB_VUART_WREN, unit), mask);
}

/**
 * @brief   Returns and clears the pending RX errors.
 *
 * @param[in] unit      VIO UART unit
 * @return              The host SIO errors mask.
 */
static inline uint32_t uartGetAndClearErrors(uint32_t unit) {

  __syscall1r(97, VIO_CALL(SB_VUART_GCERR, unit));
  return r0;
}

/**
 * @brief   Returns and clears pending events.
 *
 * @param[in] unit      VIO UART unit
 * @param[in] mask      host SIO events to be returned and cleared
 * @return              The host SIO events mask.
 */
static inline uint32_t uartGetAndClearEvents(uint32_t unit, uint32_t mask) {

  __syscall2r(97, VIO_CALL(SB_VUART_GCEVT, unit), mask);
  return r0;
}

/**
 * @brief   Returns the pending events without clearing them.
 *
 * @param[in] unit      VIO UART unit
 * @return              The host SIO events mask.
 */
static inline uint32_t uartGetEvents(uint32_t unit) {

  __syscall1r(97, VIO_CALL(SB_VUART_GEVT, unit));
  return r0;
}

/**
 * @brief   Performs a control operation on the unit.
 *
 * @param[in] unit      VIO UART unit
 * @param[in] operation host SIO control operation code
 * @param[in,out] arg   operation argument
 * @return              The operation status.
 */
static inline msg_t uartControl(uint32_t unit, uint32_t operation,
                                void *arg) {

  __syscall3r(97, VIO_CALL(SB_VUART_CTL, unit), operation, arg);
  return (msg_t)r0;
}
/** @} */

/**
 * @name    VIO SPI API
 * @note    Transfers are asynchronous, completion is signaled on the unit
 *          VRQ. Frame counts are in host frames, buffers must stay valid
 *          until completion.
 * @{
 */
/**
 * @brief   Starts an SPI unit using its default configuration.
 *
 * @param[in] unit      VIO SPI unit
 * @param[out] cfgp     buffer receiving the configuration, can be @p NULL
 * @param[in] n         size of the buffer
 * @return              The operation status.
 */
static inline msg_t spiInit(uint32_t unit, void *cfgp, size_t n) {

  __syscall3r(226, VIO_CALL(SB_VSPI_INIT, unit), n, cfgp);
  return (msg_t)r0;
}

/**
 * @brief   Stops an SPI unit.
 *
 * @param[in] unit      VIO SPI unit
 * @return              The operation status.
 */
static inline msg_t spiDeinit(uint32_t unit) {

  __syscall1r(226, VIO_CALL(SB_VSPI_DEINIT, unit));
  return (msg_t)r0;
}

/**
 * @brief   Selects an SPI configuration.
 *
 * @param[in] unit      VIO SPI unit
 * @param[in] ncfg      configuration number
 * @param[out] cfgp     buffer receiving the configuration, can be @p NULL
 * @param[in] n         size of the buffer
 * @return              The operation status.
 */
static inline msg_t spiSelectCfg(uint32_t unit, uint32_t ncfg,
                                 void *cfgp, size_t n) {

  __syscall4r(98, VIO_CALL(SB_VSPI_SELCFG, unit), ncfg, n, cfgp);
  return (msg_t)r0;
}

/**
 * @brief   Asserts the slave select signal.
 *
 * @param[in] unit      VIO SPI unit
 * @return              The operation status.
 */
static inline msg_t spiSelect(uint32_t unit) {

  __syscall1r(98, VIO_CALL(SB_VSPI_SELECT, unit));
  return (msg_t)r0;
}

/**
 * @brief   De-asserts the slave select signal.
 *
 * @param[in] unit      VIO SPI unit
 * @return              The operation status.
 */
static inline msg_t spiUnselect(uint32_t unit) {

  __syscall1r(98, VIO_CALL(SB_VSPI_UNSELECT, unit));
  return (msg_t)r0;
}

/**
 * @brief   Starts transmitting dummy frames ignoring the received data.
 *
 * @param[in] unit      VIO SPI unit
 * @param[in] n         number of frames
 * @return              The operation status.
 */
static inline msg_t spiStartIgnore(uint32_t unit, size_t n) {

  __syscall2r(98, VIO_CALL(SB_VSPI_PULSES, unit), n);
  return (msg_t)r0;
}

/**
 * @brief   Starts a receive-only transfer.
 *
 * @param[in] unit      VIO SPI unit
 * @param[in] n         number of frames
 * @param[out] rxbuf    receive buffer
 * @return              The operation status.
 */
static inline msg_t spiStartReceive(uint32_t unit, size_t n, void *rxbuf) {

  __syscall3r(98, VIO_CALL(SB_VSPI_RECEIVE, unit), n, rxbuf);
  return (msg_t)r0;
}

/**
 * @brief   Starts a transmit-only transfer.
 *
 * @param[in] unit      VIO SPI unit
 * @param[in] n         number of frames
 * @param[in] txbuf     transmit buffer
 * @return              The operation status.
 */
static inline msg_t spiStartSend(uint32_t unit, size_t n,
                                 const void *txbuf) {

  __syscall3r(98, VIO_CALL(SB_VSPI_SEND, unit), n, txbuf);
  return (msg_t)r0;
}

/**
 * @brief   Starts a full duplex transfer.
 *
 * @param[in] unit      VIO SPI unit
 * @param[in] n         number of frames
 * @param[in] txbuf     transmit buffer
 * @param[out] rxbuf    receive buffer
 * @return              The operation status.
 */
static inline msg_t spiStartExchange(uint32_t unit, size_t n,
                                     const void *txbuf, void *rxbuf) {

  __syscall4r(98, VIO_CALL(SB_VSPI_EXCHANGE, unit), n, txbuf, rxbuf);
  return (msg_t)r0;
}

/**
 * @brief   Stops the ongoing transfer.
 *
 * @param[in] unit      VIO SPI unit
 * @param[out] np       number of frames not transferred, must not be
 *                      @p NULL
 * @return              The operation status.
 */
static inline msg_t spiStopTransfer(uint32_t unit, size_t *np) {

  __syscall2r(98, VIO_CALL(SB_VSPI_STOP, unit), np);
  return (msg_t)r0;
}
/** @} */

/**
 * @name    VIO I2C API
 * @note    Transfers are asynchronous, completion is signaled on the unit
 *          VRQ. The transfer descriptor is read when the transfer starts,
 *          the buffers must stay valid until completion.
 * @{
 */
/**
 * @brief   Starts an I2C unit using its default configuration.
 *
 * @param[in] unit      VIO I2C unit
 * @param[out] cfgp     buffer receiving the configuration, can be @p NULL
 * @param[in] n         size of the buffer
 * @return              The operation status.
 */
static inline msg_t i2cInit(uint32_t unit, void *cfgp, size_t n) {

  __syscall3r(230, VIO_CALL(SB_VI2C_INIT, unit), n, cfgp);
  return (msg_t)r0;
}

/**
 * @brief   Stops an I2C unit.
 *
 * @param[in] unit      VIO I2C unit
 * @return              The operation status.
 */
static inline msg_t i2cDeinit(uint32_t unit) {

  __syscall1r(230, VIO_CALL(SB_VI2C_DEINIT, unit));
  return (msg_t)r0;
}

/**
 * @brief   Selects an I2C configuration.
 *
 * @param[in] unit      VIO I2C unit
 * @param[in] ncfg      configuration number
 * @return              The operation status.
 */
static inline msg_t i2cSelectCfg(uint32_t unit, uint32_t ncfg) {

  __syscall2r(102, VIO_CALL(SB_VI2C_SELCFG, unit), ncfg);
  return (msg_t)r0;
}

/**
 * @brief   Starts a master transmission, optionally followed by a
 *          reception.
 *
 * @param[in] unit      VIO I2C unit
 * @param[in] tp        transfer descriptor, @p rxbytes can be zero
 * @return              The operation status.
 */
static inline msg_t i2cStartMasterTransmit(uint32_t unit,
                                           const sb_vi2c_transfer_t *tp) {

  __syscall2r(102, VIO_CALL(SB_VI2C_TX, unit), tp);
  return (msg_t)r0;
}

/**
 * @brief   Starts a master reception.
 *
 * @param[in] unit      VIO I2C unit
 * @param[in] tp        transfer descriptor, the TX fields are ignored
 * @return              The operation status.
 */
static inline msg_t i2cStartMasterReceive(uint32_t unit,
                                          const sb_vi2c_transfer_t *tp) {

  __syscall2r(102, VIO_CALL(SB_VI2C_RX, unit), tp);
  return (msg_t)r0;
}

/**
 * @brief   Stops the ongoing transfer.
 *
 * @param[in] unit      VIO I2C unit
 * @return              The operation status.
 */
static inline msg_t i2cStopTransfer(uint32_t unit) {

  __syscall1r(102, VIO_CALL(SB_VI2C_STOP, unit));
  return (msg_t)r0;
}

/**
 * @brief   Returns and clears the errors of the last transfer.
 *
 * @param[in] unit      VIO I2C unit
 * @return              The host I2C errors mask.
 */
static inline uint32_t i2cGetAndClearErrors(uint32_t unit) {

  __syscall1r(102, VIO_CALL(SB_VI2C_GCERR, unit));
  return r0;
}
/** @} */

/**
 * @name    VIO ADC API
 * @note    Conversions are asynchronous, completion is signaled on the
 *          unit VRQ. The samples buffer must stay valid until completion.
 * @{
 */
/**
 * @brief   Starts an ADC unit using its default configuration.
 *
 * @param[in] unit      VIO ADC unit
 * @param[out] cfgp     buffer receiving the configuration, can be @p NULL
 * @param[in] n         size of the buffer
 * @return              The operation status.
 */
static inline msg_t adcInit(uint32_t unit, void *cfgp, size_t n) {

  __syscall3r(228, VIO_CALL(SB_VADC_INIT, unit), n, cfgp);
  return (msg_t)r0;
}

/**
 * @brief   Stops an ADC unit.
 *
 * @param[in] unit      VIO ADC unit
 * @return              The operation status.
 */
static inline msg_t adcDeinit(uint32_t unit) {

  __syscall1r(228, VIO_CALL(SB_VADC_DEINIT, unit));
  return (msg_t)r0;
}

/**
 * @brief   Selects an ADC configuration.
 *
 * @param[in] unit      VIO ADC unit
 * @param[in] ncfg      configuration number
 * @param[out] cfgp     buffer receiving the configuration, can be @p NULL
 * @param[in] n         size of the buffer
 * @return              The operation status.
 */
static inline msg_t adcSelectCfg(uint32_t unit, uint32_t ncfg,
                                 void *cfgp, size_t n) {

  __syscall4r(100, VIO_CALL(SB_VADC_SELCFG, unit), ncfg, n, cfgp);
  return (msg_t)r0;
}

/**
 * @brief   Starts a linear conversion.
 *
 * @param[in] unit      VIO ADC unit
 * @param[in] ngrp      conversion group number in the host configuration
 * @param[out] samples  samples buffer, host sample size
 * @param[in] depth     buffer depth in samples per channel
 * @return              The operation status.
 */
static inline msg_t adcStartConversionLinear(uint32_t unit, uint32_t ngrp,
                                             void *samples, size_t depth) {

  __syscall4r(100, VIO_CALL(SB_VADC_START_LINEAR, unit), ngrp, samples,
              depth);
  return (msg_t)r0;
}

/**
 * @brief   Starts a circular conversion.
 * @note    The depth must be one or an even number.
 *
 * @param[in] unit      VIO ADC unit
 * @param[in] ngrp      conversion group number in the host configuration
 * @param[out] samples  samples buffer, host sample size
 * @param[in] depth     buffer depth in samples per channel
 * @return              The operation status.
 */
static inline msg_t adcStartConversionCircular(uint32_t unit, uint32_t ngrp,
                                               void *samples, size_t depth) {

  __syscall4r(100, VIO_CALL(SB_VADC_START_CIRCULAR, unit), ngrp, samples,
              depth);
  return (msg_t)r0;
}

/**
 * @brief   Stops the ongoing conversion.
 *
 * @param[in] unit      VIO ADC unit
 * @return              The operation status.
 */
static inline msg_t adcStopConversion(uint32_t unit) {

  __syscall1r(100, VIO_CALL(SB_VADC_STOP, unit));
  return (msg_t)r0;
}

/**
 * @brief   Returns and clears the conversion errors.
 *
 * @param[in] unit      VIO ADC unit
 * @return              The host ADC errors mask.
 */
static inline uint32_t adcGetAndClearErrors(uint32_t unit) {

  __syscall1r(100, VIO_CALL(SB_VADC_GCERR, unit));
  return r0;
}
/** @} */

/**
 * @name    VIO GPT API
 * @note    Timer expirations are signaled on the unit VRQ if enabled using
 *          @p gptSetCallback().
 * @{
 */
/**
 * @brief   Starts a GPT unit using its default configuration.
 *
 * @param[in] unit      VIO GPT unit
 * @param[out] cfgp     buffer receiving the configuration, can be @p NULL
 * @param[in] n         size of the buffer
 * @return              The operation status.
 */
static inline msg_t gptInit(uint32_t unit, void *cfgp, size_t n) {

  __syscall3r(229, VIO_CALL(SB_VGPT_INIT, unit), n, cfgp);
  return (msg_t)r0;
}

/**
 * @brief   Stops a GPT unit.
 *
 * @param[in] unit      VIO GPT unit
 * @return              The operation status.
 */
static inline msg_t gptDeinit(uint32_t unit) {

  __syscall1r(229, VIO_CALL(SB_VGPT_DEINIT, unit));
  return (msg_t)r0;
}

/**
 * @brief   Selects a GPT configuration.
 *
 * @param[in] unit      VIO GPT unit
 * @param[in] ncfg      configuration number
 * @param[out] cfgp     buffer receiving the configuration, can be @p NULL
 * @param[in] n         size of the buffer
 * @return              The operation status.
 */
static inline msg_t gptSelectCfg(uint32_t unit, uint32_t ncfg,
                                 void *cfgp, size_t n) {

  __syscall4r(101, VIO_CALL(SB_VGPT_SELCFG, unit), ncfg, n, cfgp);
  return (msg_t)r0;
}

/**
 * @brief   Enables or disables the expiration VRQ.
 *
 * @param[in] unit      VIO GPT unit
 * @param[in] enable    @p true for enabling the VRQ
 * @return              The operation status.
 */
static inline msg_t gptSetCallback(uint32_t unit, bool enable) {

  __syscall2r(101, VIO_CALL(SB_VGPT_SETCB, unit), enable ? 1U : 0U);
  return (msg_t)r0;
}

/**
 * @brief   Starts the timer in continuous mode.
 *
 * @param[in] unit      VIO GPT unit
 * @param[in] interval  period in timer ticks, zero is not allowed
 * @return              The operation status.
 */
static inline msg_t gptStartContinuous(uint32_t unit, uint32_t interval) {

  __syscall3r(101, VIO_CALL(SB_VGPT_START, unit), SB_VGPT_CONTINUOUS,
              interval);
  return (msg_t)r0;
}

/**
 * @brief   Starts the timer in one shot mode.
 *
 * @param[in] unit      VIO GPT unit
 * @param[in] interval  time interval in timer ticks, zero is not allowed
 * @return              The operation status.
 */
static inline msg_t gptStartOneShot(uint32_t unit, uint32_t interval) {

  __syscall3r(101, VIO_CALL(SB_VGPT_START, unit), SB_VGPT_ONESHOT,
              interval);
  return (msg_t)r0;
}

/**
 * @brief   Stops the timer.
 *
 * @param[in] unit      VIO GPT unit
 * @return              The operation status.
 */
static inline msg_t gptStopTimer(uint32_t unit) {

  __syscall1r(101, VIO_CALL(SB_VGPT_STOP, unit));
  return (msg_t)r0;
}

/**
 * @brief   Changes the period of a timer running in continuous mode.
 *
 * @param[in] unit      VIO GPT unit
 * @param[in] interval  new period in timer ticks, zero is not allowed
 * @return              The operation status.
 */
static inline msg_t gptChangeInterval(uint32_t unit, uint32_t interval) {

  __syscall2r(101, VIO_CALL(SB_VGPT_CHGI, unit), interval);
  return (msg_t)r0;
}

/**
 * @brief   Busy-waits for the specified number of timer ticks.
 * @note    The wait is performed by the host, the guest does not run
 *          meanwhile.
 *
 * @param[in] unit      VIO GPT unit
 * @param[in] interval  time interval in timer ticks, zero is not allowed
 * @return              The operation status.
 */
static inline msg_t gptPolledDelay(uint32_t unit, uint32_t interval) {

  __syscall2r(101, VIO_CALL(SB_VGPT_PDELAY, unit), interval);
  return (msg_t)r0;
}

/**
 * @brief   Returns the current period of the timer.
 *
 * @param[in] unit      VIO GPT unit
 * @return              The period in timer ticks.
 */
static inline uint32_t gptGetInterval(uint32_t unit) {

  __syscall1r(101, VIO_CALL(SB_VGPT_GETI, unit));
  return r0;
}

/**
 * @brief   Returns the current counter value.
 *
 * @param[in] unit      VIO GPT unit
 * @return              The counter value.
 */
static inline uint32_t gptGetCounter(uint32_t unit) {

  __syscall1r(101, VIO_CALL(SB_VGPT_GETC, unit));
  return r0;
}

/**
 * @brief   Returns the timer frequency.
 *
 * @param[in] unit      VIO GPT unit
 * @return              The frequency in Hz, zero if not started.
 */
static inline uint32_t gptGetFrequency(uint32_t unit) {

  __syscall1r(101, VIO_CALL(SB_VGPT_GETFREQ, unit));
  return r0;
}
/** @} */

/**
 * @name    VIO ETH API
 * @note    Driver events are signaled on the unit VRQ, the VRQ flags contain
 *          the host ETH event flags instead of the driver state.
 * @{
 */
/**
 * @brief   Starts an ETH unit using its default configuration.
 *
 * @param[in] unit      VIO ETH unit
 * @param[out] cfgp     configuration information, can be @p NULL
 * @return              The operation status.
 */
static inline msg_t ethInit(uint32_t unit, vio_eth_config_t *cfgp) {

  __syscall3r(227, VIO_CALL(SB_VETH_INIT, unit),
              cfgp != NULL ? sizeof (vio_eth_config_t) : 0U, cfgp);
  return (msg_t)r0;
}

/**
 * @brief   Stops an ETH unit.
 *
 * @param[in] unit      VIO ETH unit
 * @return              The operation status.
 */
static inline msg_t ethDeinit(uint32_t unit) {

  __syscall1r(227, VIO_CALL(SB_VETH_DEINIT, unit));
  return (msg_t)r0;
}

/**
 * @brief   Selects an ETH configuration.
 *
 * @param[in] unit      VIO ETH unit
 * @param[in] ncfg      configuration number
 * @param[out] cfgp     configuration information, can be @p NULL
 * @return              The operation status.
 */
static inline msg_t ethSelectCfg(uint32_t unit, uint32_t ncfg,
                                 vio_eth_config_t *cfgp) {

  __syscall4r(99, VIO_CALL(SB_VETH_SELCFG, unit), ncfg,
              cfgp != NULL ? sizeof (vio_eth_config_t) : 0U, cfgp);
  return (msg_t)r0;
}

/**
 * @brief   Polls the link status.
 *
 * @param[in] unit      VIO ETH unit
 * @return              The link status.
 * @retval false        if the link is down.
 * @retval true         if the link is up.
 */
static inline bool ethPollLinkStatus(uint32_t unit) {

  __syscall1r(99, VIO_CALL(SB_VETH_LINK, unit));
  return (bool)(r0 != 0U);
}

/**
 * @brief   Returns a receive handle.
 *
 * @param[in] unit      VIO ETH unit
 * @return              The receive handle.
 * @retval 0            if no received frame is available.
 */
static inline uint32_t ethGetReceiveHandle(uint32_t unit) {

  __syscall1r(99, VIO_CALL(SB_VETH_RXGET, unit));
  return r0;
}

/**
 * @brief   Returns a transmit handle.
 *
 * @param[in] unit      VIO ETH unit
 * @return              The transmit handle.
 * @retval 0            if no transmit buffer is available.
 */
static inline uint32_t ethGetTransmitHandle(uint32_t unit) {

  __syscall1r(99, VIO_CALL(SB_VETH_TXGET, unit));
  return r0;
}

/**
 * @brief   Reads data from a receive handle.
 *
 * @param[in] unit      VIO ETH unit
 * @param[in] rxh       receive handle
 * @param[out] buffer   buffer for the received data
 * @param[in] n         maximum number of bytes to read
 * @return              The number of bytes read.
 */
static inline size_t ethReadReceiveHandle(uint32_t unit, uint32_t rxh,
                                          uint8_t *buffer, size_t n) {

  __syscall4r(99, VIO_CALL(SB_VETH_RXREAD, unit), rxh, buffer, n);
  return (size_t)r0;
}

/**
 * @brief   Writes data into a transmit handle.
 *
 * @param[in] unit      VIO ETH unit
 * @param[in] txh       transmit handle
 * @param[in] buffer    buffer containing the data to be transmitted
 * @param[in] n         maximum number of bytes to write
 * @return              The number of bytes written.
 */
static inline size_t ethWriteTransmitHandle(uint32_t unit, uint32_t txh,
                                            const uint8_t *buffer,
                                            size_t n) {

  __syscall4r(99, VIO_CALL(SB_VETH_TXWRITE, unit), txh, buffer, n);
  return (size_t)r0;
}

/**
 * @brief   Releases a receive handle.
 *
 * @param[in] unit      VIO ETH unit
 * @param[in] rxh       receive handle
 * @return              The operation status.
 */
static inline msg_t ethReleaseReceiveHandle(uint32_t unit, uint32_t rxh) {

  __syscall2r(99, VIO_CALL(SB_VETH_RXREL, unit), rxh);
  return (msg_t)r0;
}

/**
 * @brief   Releases a transmit handle, the frame is transmitted.
 *
 * @param[in] unit      VIO ETH unit
 * @param[in] txh       transmit handle
 * @return              The operation status.
 */
static inline msg_t ethReleaseTransmitHandle(uint32_t unit, uint32_t txh) {

  __syscall2r(99, VIO_CALL(SB_VETH_TXREL, unit), txh);
  return (msg_t)r0;
}
/** @} */

#endif /* SBUSERVIO_H */

/** @} */
