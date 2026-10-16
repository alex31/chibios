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
 * @file    sbvio.c
 * @brief   ARM SandBox host Virtual I/O code.
 *
 * @addtogroup ARM_SANDBOX_HOST_VIO
 * @{
 */

#include "sb.h"

#if (SB_CFG_ENABLE_VIO == TRUE) || defined(__DOXYGEN__)

/*===========================================================================*/
/* Module local definitions.                                                 */
/*===========================================================================*/

/* The VIO return codes and VRQ flags are part of the sandbox ABI, they must
   match the host definitions.*/
__CH_STATIC_ASSERT(sb_vio_ret_codes,
  ((SB_VIO_RET_SUCCESS == HAL_RET_SUCCESS) &&
   (SB_VIO_RET_CONFIG_ERROR == HAL_RET_CONFIG_ERROR) &&
   (SB_VIO_RET_NO_RESOURCE == HAL_RET_NO_RESOURCE) &&
   (SB_VIO_RET_HW_BUSY == HAL_RET_HW_BUSY) &&
   (SB_VIO_RET_HW_FAILURE == HAL_RET_HW_FAILURE) &&
   (SB_VIO_RET_UNKNOWN_CTL == HAL_RET_UNKNOWN_CTL) &&
   (SB_VIO_RET_IS_INVALID == HAL_RET_IS_INVALID) &&
   (SB_VIO_RET_INV_STATE == HAL_RET_INV_STATE)));

__CH_STATIC_ASSERT(sb_vio_vrq_flags,
  ((SB_VIO_FL_HALF == (1U << HAL_DRV_STATE_HALF)) &&
   (SB_VIO_FL_FULL == (1U << HAL_DRV_STATE_FULL)) &&
   (SB_VIO_FL_COMPLETE == (1U << HAL_DRV_STATE_COMPLETE)) &&
   (SB_VIO_FL_ERROR == (1U << HAL_DRV_STATE_ERROR))));

/*===========================================================================*/
/* Module exported variables.                                                */
/*===========================================================================*/

/*===========================================================================*/
/* Module local types.                                                       */
/*===========================================================================*/

/*===========================================================================*/
/* Module local variables.                                                   */
/*===========================================================================*/

/*===========================================================================*/
/* Module local functions.                                                   */
/*===========================================================================*/

/*===========================================================================*/
/* Module exported functions.                                                */
/*===========================================================================*/

void __sb_vio_cleanup(sb_class_t *sbp) {
  const vio_conf_t *vioconf;

  vioconf = sbp->vioconf;
  if (vioconf == NULL) {
    return;
  }

#if VIO_CFG_ENABLE_ADC == TRUE
  if (vioconf->adcs != NULL) {
    unsigned i;

    for (i = 0U; i < vioconf->adcs->n; i++) {
      const vio_adc_unit_t *unitp = &vioconf->adcs->units[i];

      drvSetCallbackX(unitp->adcp, NULL);
      drvStop(unitp->adcp);
      drvSetArgumentX(unitp->adcp, NULL);
    }
  }
#endif

#if VIO_CFG_ENABLE_ETH == TRUE
  if (vioconf->eths != NULL) {
    unsigned i;

    for (i = 0U; i < vioconf->eths->n; i++) {
      const vio_eth_unit_t *unitp = &vioconf->eths->units[i];

      drvSetCallbackX(unitp->ethp, NULL);
      drvStop(unitp->ethp);
      drvSetArgumentX(unitp->ethp, NULL);
    }
  }
#endif

#if VIO_CFG_ENABLE_GPT == TRUE
  if (vioconf->gpts != NULL) {
    unsigned i;

    for (i = 0U; i < vioconf->gpts->n; i++) {
      const vio_gpt_unit_t *unitp = &vioconf->gpts->units[i];

      drvSetCallbackX(unitp->gptp, NULL);
      drvStop(unitp->gptp);
      drvSetArgumentX(unitp->gptp, NULL);
    }
  }
#endif

#if VIO_CFG_ENABLE_I2C == TRUE
  if (vioconf->i2cs != NULL) {
    unsigned i;

    for (i = 0U; i < vioconf->i2cs->n; i++) {
      const vio_i2c_unit_t *unitp = &vioconf->i2cs->units[i];

      drvSetCallbackX(unitp->i2cp, NULL);
      drvStop(unitp->i2cp);
      drvSetArgumentX(unitp->i2cp, NULL);
    }
  }
#endif

#if VIO_CFG_ENABLE_SPI == TRUE
  if (vioconf->spis != NULL) {
    unsigned i;

    for (i = 0U; i < vioconf->spis->n; i++) {
      const vio_spi_unit_t *unitp = &vioconf->spis->units[i];

      drvSetCallbackX(unitp->spip, NULL);
      drvStop(unitp->spip);
      drvSetArgumentX(unitp->spip, NULL);
    }
  }
#endif

#if VIO_CFG_ENABLE_UART == TRUE
  if (vioconf->uarts != NULL) {
    unsigned i;

    for (i = 0U; i < vioconf->uarts->n; i++) {
      const vio_uart_unit_t *unitp = &vioconf->uarts->units[i];

      drvSetCallbackX(unitp->siop, NULL);
      drvStop(unitp->siop);
      drvSetArgumentX(unitp->siop, NULL);
    }
  }
#endif
}

#endif /* SB_CFG_ENABLE_VIO == TRUE */

/** @} */
