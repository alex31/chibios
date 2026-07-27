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
/*
   Concepts and parts of this file have been contributed by Uladzimir Pylinsky
   aka barthess.
 */

/**
 * @file    hal_i2c.h
 * @brief   I2C Driver macros and structures.
 *
 * @addtogroup I2C
 * @{
 */

#ifndef HAL_I2C_H
#define HAL_I2C_H

#if (HAL_USE_I2C == TRUE) || defined(__DOXYGEN__)

/*===========================================================================*/
/* Driver constants.                                                         */
/*===========================================================================*/

/* TODO: To be reviewed, too STM32-centric.*/
/**
 * @name    I2C bus error conditions
 * @{
 */
#define I2C_NO_ERROR               0x00    /**< @brief No error.            */
#define I2C_BUS_ERROR              0x01    /**< @brief Bus Error.           */
#define I2C_ARBITRATION_LOST       0x02    /**< @brief Arbitration Lost.    */
#define I2C_ACK_FAILURE            0x04    /**< @brief Acknowledge Failure. */
#define I2C_OVERRUN                0x08    /**< @brief Overrun/Underrun.    */
#define I2C_PEC_ERROR              0x10    /**< @brief PEC Error in
                                                reception.                  */
#define I2C_TIMEOUT                0x20    /**< @brief Hardware timeout.    */
#define I2C_SMB_ALERT              0x40    /**< @brief SMBus Alert.         */
/** @} */

/*===========================================================================*/
/* Driver pre-compile time settings.                                         */
/*===========================================================================*/

/**
 * @brief   Enables the mutual exclusion APIs on the I2C bus.
 */
#if !defined(I2C_USE_MUTUAL_EXCLUSION) || defined(__DOXYGEN__)
#define I2C_USE_MUTUAL_EXCLUSION            TRUE
#endif

/**
 * @brief   Slave mode API enable switch.
 * @note    The low level driver must support this capability.
 */
#if !defined(I2C_ENABLE_SLAVE_MODE)
#define I2C_ENABLE_SLAVE_MODE               FALSE
#endif

/*===========================================================================*/
/* Derived constants and error checks.                                       */
/*===========================================================================*/

/*===========================================================================*/
/* Driver data structures and types.                                         */
/*===========================================================================*/

/**
 * @brief   Driver state machine possible states.
 */
typedef enum {
  I2C_UNINIT = 0,                           /**< @brief Not initialized.    */
  I2C_STOP = 1,                             /**< @brief Stopped.            */
  I2C_READY = 2,                            /**< @brief Ready.              */
  I2C_ACTIVE_TX = 3,                        /**< @brief Transmitting.       */
  I2C_ACTIVE_RX = 4,                        /**< @brief Receiving.          */
  I2C_LOCKED = 5                            /**< @brief Bus locked.         */
} i2cstate_t;

#include "hal_i2c_lld.h"

/* For compatibility, some LLDs could not export this.*/
#if !defined(I2C_SUPPORTS_SLAVE_MODE)
#define I2C_SUPPORTS_SLAVE_MODE             FALSE
#endif

/* For compatibility, LLDs default to 7-bit addressing. */
#if !defined(I2C_LLD_MIN_ADDRESS)
#define I2C_LLD_MIN_ADDRESS                 0U
#endif

#if !defined(I2C_LLD_MAX_ADDRESS)
#define I2C_LLD_MAX_ADDRESS                 0x7FU
#endif

#if (I2C_LLD_MAX_ADDRESS != 0x7FU) && (I2C_LLD_MAX_ADDRESS != 0x3FFU)
#error "I2C_LLD_MAX_ADDRESS must select 7-bit or 10-bit addressing"
#endif

#if I2C_LLD_MIN_ADDRESS > I2C_LLD_MAX_ADDRESS
#error "I2C_LLD_MIN_ADDRESS must not exceed I2C_LLD_MAX_ADDRESS"
#endif

#if !defined(i2c_lld_is_config_matching)
#define i2c_lld_is_config_matching(config)  true
#endif

#if I2C_LLD_MIN_ADDRESS == 0U
#define i2c_lld_is_address_valid(addr)                                  \
  ((uint32_t)(addr) <= (uint32_t)I2C_LLD_MAX_ADDRESS)
#else
#define i2c_lld_is_address_valid(addr)                                  \
  (((uint32_t)(addr) >= (uint32_t)I2C_LLD_MIN_ADDRESS) &&               \
   ((uint32_t)(addr) <= (uint32_t)I2C_LLD_MAX_ADDRESS))
#endif

#if (I2C_SUPPORTS_SLAVE_MODE == FALSE) && (I2C_ENABLE_SLAVE_MODE == TRUE)
#error "I2C slave mode not supported"
#endif

/*===========================================================================*/
/* Driver macros.                                                            */
/*===========================================================================*/

/**
 * @name    Macro Functions
 * @{
 */
/**
 * @brief   Wakes up the waiting thread notifying no errors.
 *
 * @param[in] i2cp      pointer to the @p I2CDriver object
 *
 * @notapi
 */
#define _i2c_wakeup_isr(i2cp) do {                                          \
  osalSysLockFromISR();                                                     \
  osalThreadResumeI(&(i2cp)->thread, MSG_OK);                               \
  osalSysUnlockFromISR();                                                   \
} while (0)

/**
 * @brief   Wakes up the waiting thread notifying errors.
 *
 * @param[in] i2cp      pointer to the @p I2CDriver object
 *
 * @notapi
 */
#define _i2c_wakeup_error_isr(i2cp) do {                                    \
  osalSysLockFromISR();                                                     \
  osalThreadResumeI(&(i2cp)->thread, MSG_RESET);                            \
  osalSysUnlockFromISR();                                                   \
} while (0)

/**
 * @brief   Wrap i2cMasterTransmitTimeout function with TIME_INFINITE timeout.
 * @api
 */
#define i2cMasterTransmit(i2cp, addr, txbuf, txbytes, rxbuf, rxbytes)       \
  (i2cMasterTransmitTimeout(i2cp, addr, txbuf, txbytes, rxbuf, rxbytes,     \
                            TIME_INFINITE))

/**
 * @brief   Wrap i2cMasterReceiveTimeout function with TIME_INFINITE timeout.
 * @api
 */
#define i2cMasterReceive(i2cp, addr, rxbuf, rxbytes)                        \
  (i2cMasterReceiveTimeout(i2cp, addr, rxbuf, rxbytes, TIME_INFINITE))

#if (I2C_ENABLE_SLAVE_MODE == TRUE) || defined(__DOXYGEN__)
/**
 * @brief   Answer required.
 * @note    This function is meant to be called after slave receive only.
 *
 * @param[in] i2cp      pointer to the @p I2CDriver object
 * @return              Slave answer required.
 * @retval              false if the slave must not answer.
 * @retval              true if the slave must answer.
 *
 * @special
 */
#define i2cSlaveIsAnswerRequired(i2cp) (((i2cp)->reply_required))
#endif
/** @} */

/*===========================================================================*/
/* External declarations.                                                    */
/*===========================================================================*/

#ifdef __cplusplus
extern "C" {
#endif
  void i2cInit(void);
  void i2cObjectInit(I2CDriver *i2cp);
  msg_t i2cStart(I2CDriver *i2cp, const I2CConfig *config);
  void i2cStop(I2CDriver *i2cp);
  i2cflags_t i2cGetErrors(I2CDriver *i2cp);

  CC_NODISCARD_MSG("I2C : testez le msg_t retourne (MSG_OK, MSG_TIMEOUT, etc.)")
  OSAL_ACCESS_WO(5, 6) OSAL_NONNULL_IF_NONZERO(5, 6)
  OSAL_ACCESS_RO(3, 4) OSAL_NONNULL_IF_NONZERO(3, 4)
  msg_t i2cMasterTransmitTimeout(I2CDriver *i2cp,
                                 i2caddr_t addr,
                                 const void *txbuf, size_t txbytes,
                                 void *rxbuf, size_t rxbytes,
                                 sysinterval_t timeout);

  CC_NODISCARD_MSG("I2C : testez le msg_t retourne (MSG_OK, MSG_TIMEOUT, etc.)")
  OSAL_ACCESS_WO(3, 4) OSAL_NONNULL_IF_NONZERO(3, 4)
  msg_t i2cMasterReceiveTimeout(I2CDriver *i2cp,
                                i2caddr_t addr,
                                void *rxbuf, size_t rxbytes,
                                sysinterval_t timeout);
#if I2C_USE_MUTUAL_EXCLUSION == TRUE
  void i2cAcquireBus(I2CDriver *i2cp);
  void i2cReleaseBus(I2CDriver *i2cp);
#endif

#if I2C_ENABLE_SLAVE_MODE == TRUE
  [[nodiscard]]
  msg_t i2cSlaveMatchAddress(I2CDriver *i2cp, i2caddr_t  i2cadr);
  [[nodiscard]]
  OSAL_ACCESS_WO(3, 4) OSAL_NONNULL_IF_NONZERO(3, 4)
  msg_t i2cSlaveReceiveTimeout(I2CDriver *i2cp, void *rxbuf,
                               size_t rxbytes, sysinterval_t timeout);
  [[nodiscard]]
  OSAL_ACCESS_RO(3, 4) OSAL_NONNULL_IF_NONZERO(3, 4)
  msg_t i2cSlaveTransmitTimeout(I2CDriver *i2cp, const void *txbuf,
                                size_t txbytes, sysinterval_t timeout);
#endif
#ifdef __cplusplus
}
#endif

#if CC_HAS_CONSTEXPR_ERROR
#if I2C_LLD_MAX_ADDRESS == 0x7FU
CC_CONSTEXPR_ERROR(__i2c_invalid_master_address_constant,
                   "adresse I2C invalide : adresse 7 bits attendue");
#else
CC_CONSTEXPR_ERROR(__i2c_invalid_master_address_constant,
                   "adresse I2C invalide : adresse 10 bits attendue");
#endif
CC_CONSTEXPR_ERROR(__i2c_zero_transfer_size_constant,
                   "taille I2C invalide : au moins un octet attendu");
CC_CONSTEXPR_ERROR(__i2c_immediate_timeout_constant,
                   "timeout I2C invalide : TIME_IMMEDIATE est interdit");
#define i2cDbgCheckMasterTransmitX(addr, txbytes, timeout)                  \
  (CC_CONSTEXPR_CHECK(addr,                                                \
                      !i2c_lld_is_address_valid(                           \
                        CC_CONSTEXPR_VALUE(addr, I2C_LLD_MIN_ADDRESS)),    \
                      __i2c_invalid_master_address_constant),              \
   CC_CONSTEXPR_CHECK(txbytes,                                             \
                      (size_t)(txbytes) == 0U,                             \
                      __i2c_zero_transfer_size_constant),                  \
   CC_CONSTEXPR_CHECK(timeout,                                             \
                      (sysinterval_t)(timeout) == TIME_IMMEDIATE,          \
                      __i2c_immediate_timeout_constant))
#define i2cDbgCheckMasterReceiveX(addr, rxbytes, timeout)                   \
  (CC_CONSTEXPR_CHECK(addr,                                                \
                      ((uint32_t)CC_CONSTEXPR_VALUE(addr, 1U) == 0U) ||    \
                      !i2c_lld_is_address_valid(                           \
                        CC_CONSTEXPR_VALUE(addr, I2C_LLD_MIN_ADDRESS)),    \
                      __i2c_invalid_master_address_constant),              \
   CC_CONSTEXPR_CHECK(rxbytes,                                             \
                      (size_t)(rxbytes) == 0U,                             \
                      __i2c_zero_transfer_size_constant),                  \
   CC_CONSTEXPR_CHECK(timeout,                                             \
                      (sysinterval_t)(timeout) == TIME_IMMEDIATE,          \
                      __i2c_immediate_timeout_constant))
#define i2cMasterTransmitTimeout(i2cp, addr, txbuf, txbytes,                \
                                 rxbuf, rxbytes, timeout)                   \
  (i2cDbgCheckMasterTransmitX(addr, txbytes, timeout),                     \
   (i2cMasterTransmitTimeout)(i2cp, addr, txbuf, txbytes,                  \
                              rxbuf, rxbytes, timeout))
#define i2cMasterReceiveTimeout(i2cp, addr, rxbuf, rxbytes, timeout)        \
  (i2cDbgCheckMasterReceiveX(addr, rxbytes, timeout),                      \
   (i2cMasterReceiveTimeout)(i2cp, addr, rxbuf, rxbytes, timeout))
#endif

#endif /* HAL_USE_I2C == TRUE */

#endif /* HAL_I2C_H */

/** @} */
