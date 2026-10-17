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
 * @file    sb/host/sbhost.c
 * @brief   ARM SandBox syscall dispatcher code.
 *
 * @addtogroup ARM_SANDBOX_HOSTAPI
 * @{
 */

#include "ch.h"
#include "sb.h"

/*===========================================================================*/
/* Module local definitions.                                                 */
/*===========================================================================*/

/*===========================================================================*/
/* Module exported variables.                                                */
/*===========================================================================*/

static void sb_undef_handler(sb_class_t *sbp, struct port_extctx *ectxp);

/* The tables are initialized to the not implemented handler, then the
   assigned entries override it.*/
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Woverride-init"

/**
 * @brief   Fastcalls table.
 * @note    Entries are indexed by the @p SB_FASTC_xxx codes, unassigned
 *          entries and entries of disabled services are served as not
 *          implemented.
 */
const port_syscall_t sb_fastcalls[SB_SYSC_BASE] = {
  [0 ... SB_SYSC_BASE - 1]                = sb_undef_handler,
  [SB_FASTC_GET_SYSTIME]                  = sb_fastc_get_systime,
  [SB_FASTC_GET_FREQUENCY]                = sb_fastc_get_frequency,
  [SB_FASTC_EVENT_BROADCAST]              = sb_fastc_broadcast_flags,
  [SB_FASTC_NOP]                          = sb_fastc_nop,
#if SB_CFG_ENABLE_VIO == TRUE
#if VIO_CFG_ENABLE_GPIO == TRUE
  [SB_FASTC_VIO_VGPIO]                    = sb_fastc_vio_gpio,
#endif
#if VIO_CFG_ENABLE_UART == TRUE
  [SB_FASTC_VIO_VUART]                    = sb_fastc_vio_uart,
#endif
#if VIO_CFG_ENABLE_SPI == TRUE
  [SB_FASTC_VIO_VSPI]                     = sb_fastc_vio_spi,
#endif
#if VIO_CFG_ENABLE_ETH == TRUE
  [SB_FASTC_VIO_VETH]                     = sb_fastc_vio_eth,
#endif
#if VIO_CFG_ENABLE_ADC == TRUE
  [SB_FASTC_VIO_VADC]                     = sb_fastc_vio_adc,
#endif
#if VIO_CFG_ENABLE_GPT == TRUE
  [SB_FASTC_VIO_VGPT]                     = sb_fastc_vio_gpt,
#endif
#if VIO_CFG_ENABLE_I2C == TRUE
  [SB_FASTC_VIO_VI2C]                     = sb_fastc_vio_i2c,
#endif
#endif
#if SB_CFG_ENABLE_VRQ == TRUE
  [SB_FASTC_VRQ_SET_ALARM]                = sb_fastc_vrq_set_alarm,
  [SB_FASTC_VRQ_RESET_ALARM]              = sb_fastc_vrq_reset_alarm,
  [SB_FASTC_VRQ_GCSTS]                    = sb_fastc_vrq_gcsts,
  [SB_FASTC_VRQ_SETWT]                    = sb_fastc_vrq_setwt,
  [SB_FASTC_VRQ_CLRWT]                    = sb_fastc_vrq_clrwt,
  [SB_FASTC_VRQ_SETEN]                    = sb_fastc_vrq_seten,
  [SB_FASTC_VRQ_CLREN]                    = sb_fastc_vrq_clren,
  [SB_FASTC_VRQ_DISABLE]                  = sb_fastc_vrq_disable,
  [SB_FASTC_VRQ_ENABLE]                   = sb_fastc_vrq_enable,
  [SB_FASTC_VRQ_GETISR]                   = sb_fastc_vrq_getisr,
  [SB_FASTC_VRQ_RETURN]                   = sb_fastc_vrq_return,
#endif
};

/**
 * @brief   Syscalls table.
 * @note    Entries are indexed by the @p SB_SYSC_xxx codes minus
 *          @p SB_SYSC_BASE, unassigned entries and entries of disabled
 *          services are served as not implemented.
 */
const port_syscall_t sb_syscalls[256 - SB_SYSC_BASE] = {
  [0 ... 255 - SB_SYSC_BASE]              = sb_undef_handler,
#if SB_CFG_ENABLE_VFS == TRUE
  [SB_SYSC_POSIX - SB_SYSC_BASE]          = sb_sysc_stdio,
#endif
  [SB_SYSC_EXIT - SB_SYSC_BASE]           = sb_sysc_exit,
  [SB_SYSC_SLEEP - SB_SYSC_BASE]          = sb_sysc_sleep,
  [SB_SYSC_SLEEP_UNTIL - SB_SYSC_BASE]    = sb_sysc_sleep_until_windowed,
  [SB_SYSC_MESSAGE_WAIT - SB_SYSC_BASE]   = sb_sysc_wait_message,
  [SB_SYSC_MESSAGE_REPLY - SB_SYSC_BASE]  = sb_sysc_reply_message,
  [SB_SYSC_EVENT_WAIT_ONE - SB_SYSC_BASE] = sb_sysc_wait_one_timeout,
  [SB_SYSC_EVENT_WAIT_ANY - SB_SYSC_BASE] = sb_sysc_wait_any_timeout,
  [SB_SYSC_EVENT_WAIT_ALL - SB_SYSC_BASE] = sb_sysc_wait_all_timeout,
  [SB_SYSC_LOADELF - SB_SYSC_BASE]        = sb_sysc_loadelf,
  [SB_SYSC_NOP - SB_SYSC_BASE]            = sb_sysc_nop,
#if SB_CFG_ENABLE_VIO == TRUE
#if VIO_CFG_ENABLE_UART == TRUE
  [SB_SYSC_VIO_VUART - SB_SYSC_BASE]      = sb_sysc_vio_uart,
#endif
#if VIO_CFG_ENABLE_SPI == TRUE
  [SB_SYSC_VIO_VSPI - SB_SYSC_BASE]       = sb_sysc_vio_spi,
#endif
#if VIO_CFG_ENABLE_ETH == TRUE
  [SB_SYSC_VIO_VETH - SB_SYSC_BASE]       = sb_sysc_vio_eth,
#endif
#if VIO_CFG_ENABLE_ADC == TRUE
  [SB_SYSC_VIO_VADC - SB_SYSC_BASE]       = sb_sysc_vio_adc,
#endif
#if VIO_CFG_ENABLE_GPT == TRUE
  [SB_SYSC_VIO_VGPT - SB_SYSC_BASE]       = sb_sysc_vio_gpt,
#endif
#if VIO_CFG_ENABLE_I2C == TRUE
  [SB_SYSC_VIO_VI2C - SB_SYSC_BASE]       = sb_sysc_vio_i2c,
#endif
#endif
#if SB_CFG_ENABLE_VRQ == TRUE
  [SB_SYSC_VRQ_WAIT - SB_SYSC_BASE]       = sb_sysc_vrq_wait,
#endif
};

#pragma GCC diagnostic pop

/*===========================================================================*/
/* Module local types.                                                       */
/*===========================================================================*/

/*===========================================================================*/
/* Module local variables.                                                   */
/*===========================================================================*/

/*===========================================================================*/
/* Module local functions.                                                   */
/*===========================================================================*/

static void sb_undef_handler(sb_class_t *sbp, struct port_extctx *ectxp) {

  (void)sbp;

  ectxp->r0 = CH_RET_ENOSYS;
}

/*===========================================================================*/
/* Module exported functions.                                                */
/*===========================================================================*/

void __sb_abort(msg_t msg) {
  sb_class_t *sbp = (sb_class_t *)chThdGetSelfX()->object;

  chDbgAssert(sbp->state == SB_STATE_RUNNING, "invalid lifecycle state");
  sbp->state = SB_STATE_STOPPING;

#if SB_CFG_ENABLE_VRQ == TRUE
  chVTResetI(&sbp->vrq.alarm_vt);
#endif

#if SB_CFG_ENABLE_VFS == TRUE
  chSysUnlock();
  __sb_io_cleanup(sbp);
  chSysLock();
#endif
#if SB_CFG_ENABLE_VIO == TRUE
  chSysUnlock();
  __sb_vio_cleanup(sbp);
  chSysLock();
#endif

#if CH_CFG_USE_EVENTS == TRUE
  chEvtBroadcastI(&sb.termination_es);
#endif
#if CH_CFG_USE_MESSAGES == TRUE
  sbp->base.msg_tp = NULL;
  chMsgReleaseAllI();
#endif
  chThdExitS(msg);
  chSysHalt("zombies");
}

/**
 * @brief   Redefined syscall entry vector.
 * @note    When FPU is enabled there is the assumption that the syscall
 *          code does not change the FPCA state or __port_do_syscall_return()
 *          would try to return using the wrong frame type.
 */
void __port_do_syscall_entry(uint32_t n, struct port_extctx *ectxp) {
  extern void __sb_dispatch_syscall(sb_class_t *sbp,
                                    struct port_extctx *ectxp,
                                    uint32_t n);
  thread_t *tp = __sch_get_currthread();
  sb_class_t *sbp = (sb_class_t *)tp->object;
  struct port_extctx *newctxp;

#if (CORTEX_USE_FPU == TRUE) && (PORT_USE_FPU_FAST_SWITCHING > 0)
  /* Completing any lazy FP save to the caller frame before switching
     to the privileged stack.*/
  (void)__get_FPSCR();
  asm volatile ("" : : : "memory");
#endif

  /* Saving caller context in unprivileged memory.*/
  sbp->u_psp = (uint32_t)ectxp;

  /* Return context for change in privileged mode, it is created on the
     privileged PSP so no need to check for overflows.*/
  newctxp = (struct port_extctx *)(void *)((uint8_t *)tp->waend -
                                           sizeof (struct port_extctx));

  /* Creating context for return in privileged mode.*/
  newctxp->r0   = (uint32_t)sbp;
  newctxp->r1   = (uint32_t)ectxp;
  newctxp->r2   = n;
  newctxp->pc   = (uint32_t)__sb_dispatch_syscall;
  newctxp->xpsr = (uint32_t)0x01000000;
#if CORTEX_USE_FPU == TRUE
  newctxp->fpscr = FPU->FPDSCR;
#endif

  /* Switching PSP to the privileged mode PSP.*/
  __set_PSP((uint32_t)newctxp);
#if defined(PORT_ARCHITECTURE_ARM_V8M_MAINLINE)
  __set_PSPLIM((uint32_t)sbp->thread.wabase);
#endif
}

/**
 * @brief   Redefined syscall return vector with VRQ handling.
 * @note    When FPU is enabled there is the assumption that the syscall
 *          code does not change the FPCA state or this code would try to
 *          return using the wrong frame type.
 */
void __port_do_syscall_return(void) {
  thread_t *tp;
  sb_class_t *sbp;
  struct port_extctx *ectxp;

  tp = __sch_get_currthread();
  sbp = (sb_class_t *)tp->object;
  ectxp = (struct port_extctx *)sbp->u_psp;

#if CORTEX_USE_FPU == TRUE
  /* Discarding the privileged lazy FP save and forcing FP unstacking
     from the sandbox frame, including on the initial trampoline return.*/
  FPU->FPCCR &= ~FPU_FPCCR_LSPACT_Msk;
#endif

#if SB_CFG_ENABLE_VRQ == TRUE
  __sb_vrq_check_pending(sbp, ectxp);
#else
  __set_PSP((uint32_t)ectxp);
#if defined(PORT_ARCHITECTURE_ARM_V8M_MAINLINE)
  __set_PSPLIM((uint32_t)sbp->u_data->base);
#endif
#endif
}

void __port_do_fastcall_entry(uint32_t n, struct port_extctx *ectxp) {
  thread_t *tp = __sch_get_currthread();
  sb_class_t *sbp = (sb_class_t *)tp->object;

  sb_fastcalls[n](sbp, ectxp);
}

/** @} */
