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
 * @file    sb/host/sbsyscall.h
 * @brief   ARM SandBox syscall dispatcher macros and structures.
 *
 * @addtogroup ARM_SANDBOX_HOSTAPI
 * @{
 */

#ifndef SBSYSCALL_H
#define SBSYSCALL_H

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

/**
 * @brief   Type of a syscall handler.
 */
typedef void (*port_syscall_t)(sb_class_t *sbp, struct port_extctx *ectx);

/*===========================================================================*/
/* Module macros.                                                            */
/*===========================================================================*/

/**
 * @brief   Binds a handler to a fastcall or syscall code.
 * @details The handler becomes the strong definition of the table entry
 *          @p __sb_svcN, which is otherwise a weak alias of the not
 *          implemented handler. It is used by the host modules and by the
 *          application, two handlers bound to the same code are a link
 *          error.
 * @note    It must be placed in the translation unit defining the handler.
 * @note    @p n must be a decimal integer literal without suffixes, or a
 *          macro expanding to one, because it is pasted into the entry
 *          name. The codes in the ranges starting at
 *          @p SB_FASTC_USER_BASE and @p SB_SYSC_USER_BASE are never used by
 *          ChibiOS.
 *
 * @param[in] n         fastcall or syscall code
 * @param[in] handler   handler function name
 */
#define SB_SVC_HANDLER(n, handler)      __SB_SVC_HANDLER(n, handler)
#define __SB_SVC_HANDLER(n, handler)                                        \
  __CH_STATIC_ASSERT(__sb_svc##n##_code, (n) <= 255);                       \
  __CH_STATIC_ASSERT(__sb_svc##n##_type,                                    \
                     __builtin_types_compatible_p(__typeof__(&handler),     \
                                                  port_syscall_t));         \
  void __sb_svc##n(sb_class_t *sbp, struct port_extctx *ectxp)              \
    __attribute__((alias(#handler)))

/*===========================================================================*/
/* External declarations.                                                    */
/*===========================================================================*/

#ifdef __cplusplus
extern "C" {
#endif
  void __sb_cleanup(void);
  void __sb_abort(msg_t msg);
#ifdef __cplusplus
}
#endif

/*===========================================================================*/
/* Module inline functions.                                                  */
/*===========================================================================*/

#endif /* SBSYSCALL_H */

/** @} */
