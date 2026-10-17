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
 * @file    zephyr/arch/chibios_sb/sb.h
 * @brief   ChibiOS sandbox ABI for the Zephyr guest.
 * @details Fastcall and syscall wrappers used by the architecture code,
 *          the codes are the ones shared by the sandbox host and all the
 *          guests in @p os/sb/common/sbsysc.h.
 * @note    The full sandbox user library in @p os/sb/user is not used
 *          because it brings in POSIX definitions conflicting with the
 *          Zephyr C library.
 *
 * @addtogroup ZEPHYR_CHIBIOS_SB
 * @{
 */

#ifndef ZEPHYR_INCLUDE_ARCH_CHIBIOS_SB_SB_H_
#define ZEPHYR_INCLUDE_ARCH_CHIBIOS_SB_SB_H_

#if defined(_ASMLANGUAGE) && !defined(_FROM_ASM_)
#define _FROM_ASM_
#endif

#if !defined(_ASMLANGUAGE)
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <zephyr/toolchain.h>
#endif

/* Sandbox ABI codes, relative to this file because the directory also
   contains headers conflicting with the Zephyr C library.*/
#include "../../../../../../sb/common/sbsysc.h"

/*===========================================================================*/
/* Module constants.                                                         */
/*===========================================================================*/

/**
 * @brief   Number of VRQs.
 */
#define CHIBIOS_SB_VRQ_NUMBER           32

/**
 * @brief   VRQs globally disabled bit in the VRQ ISR register.
 */
#define CHIBIOS_SB_VRQ_ISR_DISABLED     1

/**
 * @brief   Sandbox exit code of a system halt, ORed with the reason.
 */
#define CHIBIOS_SB_EXIT_HALT            0xA8000000

/*===========================================================================*/
/* Module macros.                                                            */
/*===========================================================================*/

#if !defined(_ASMLANGUAGE) || defined(__DOXYGEN__)
/**
 * @name    SVC instruction wrappers
 * @note    The @p x parameter is the SVC number, it must be an integer
 *          constant expression.
 * @{
 */
#define __chibios_sb_svc0(x)                                                \
  __asm__ volatile ("svc %[nr]" : : [nr] "i" (x) : "memory")

#define __chibios_sb_svc0r(x)                                               \
  register uint32_t r0 __asm__ ("r0");                                      \
  __asm__ volatile ("svc %[nr]" : "=r" (r0) : [nr] "i" (x) : "memory")

#define __chibios_sb_svc1r(x, p1)                                           \
  register uint32_t r0 __asm__ ("r0") = (uint32_t)(p1);                     \
  __asm__ volatile ("svc %[nr]" : "=r" (r0) : [nr] "i" (x), "r" (r0) :      \
                    "memory")

#define __chibios_sb_svc2r(x, p1, p2)                                       \
  register uint32_t r0 __asm__ ("r0") = (uint32_t)(p1);                     \
  register uint32_t r1 __asm__ ("r1") = (uint32_t)(p2);                     \
  __asm__ volatile ("svc %[nr]" : "=r" (r0) :                               \
                    [nr] "i" (x), "r" (r0), "r" (r1) : "memory")

#define __chibios_sb_svc4r(x, p1, p2, p3, p4)                               \
  register uint32_t r0 __asm__ ("r0") = (uint32_t)(p1);                     \
  register uint32_t r1 __asm__ ("r1") = (uint32_t)(p2);                     \
  register uint32_t r2 __asm__ ("r2") = (uint32_t)(p3);                     \
  register uint32_t r3 __asm__ ("r3") = (uint32_t)(p4);                     \
  __asm__ volatile ("svc %[nr]" : "=r" (r0) :                               \
                    [nr] "i" (x), "r" (r0), "r" (r1), "r" (r2), "r" (r3) :  \
                    "memory")
/** @} */
#endif /* !defined(_ASMLANGUAGE) */

/*===========================================================================*/
/* External declarations.                                                    */
/*===========================================================================*/

/*===========================================================================*/
/* Module inline functions.                                                  */
/*===========================================================================*/

#if !defined(_ASMLANGUAGE) || defined(__DOXYGEN__)
/**
 * @brief   Returns the VRQ ISR register.
 *
 * @return              The VRQ ISR register value.
 */
static ALWAYS_INLINE uint32_t chibios_sb_vrq_getisr(void) {

  __chibios_sb_svc0r(SB_FASTC_VRQ_GETISR);
  return r0;
}

/**
 * @brief   Disables VRQs globally.
 */
static ALWAYS_INLINE void chibios_sb_vrq_disable(void) {

  __chibios_sb_svc0(SB_FASTC_VRQ_DISABLE);
}

/**
 * @brief   Enables VRQs globally.
 * @details A pending and enabled VRQ is served before returning.
 */
static ALWAYS_INLINE void chibios_sb_vrq_enable(void) {

  __chibios_sb_svc0(SB_FASTC_VRQ_ENABLE);
}

/**
 * @brief   Enables the VRQs in a mask.
 *
 * @param[in] mask      mask of the VRQs to be enabled
 * @return              The previous VRQ enable mask.
 */
static ALWAYS_INLINE uint32_t chibios_sb_vrq_seten(uint32_t mask) {

  __chibios_sb_svc1r(SB_FASTC_VRQ_SETEN, mask);
  return r0;
}

/**
 * @brief   Disables the VRQs in a mask.
 *
 * @param[in] mask      mask of the VRQs to be disabled
 * @return              The previous VRQ enable mask.
 */
static ALWAYS_INLINE uint32_t chibios_sb_vrq_clren(uint32_t mask) {

  __chibios_sb_svc1r(SB_FASTC_VRQ_CLREN, mask);
  return r0;
}

/**
 * @brief   Triggers the VRQs in a mask on the guest itself.
 *
 * @param[in] mask      mask of the VRQs to be triggered
 * @return              The previous VRQ wait mask.
 */
static ALWAYS_INLINE uint32_t chibios_sb_vrq_setwt(uint32_t mask) {

  __chibios_sb_svc1r(SB_FASTC_VRQ_SETWT, mask);
  return r0;
}

/**
 * @brief   Releases the CPU to the host until a VRQ is pending.
 * @note    It returns also if VRQs are globally disabled, the pending VRQ
 *          is served when VRQs are enabled again.
 */
static ALWAYS_INLINE void chibios_sb_vrq_wait(void) {

  __chibios_sb_svc0(SB_SYSC_VRQ_WAIT);
}

/**
 * @brief   Returns the host system time.
 *
 * @return              The host system time.
 */
static ALWAYS_INLINE uint32_t chibios_sb_get_systime(void) {

  __chibios_sb_svc0r(SB_FASTC_GET_SYSTIME);
  return r0;
}

/**
 * @brief   Returns the host system time frequency.
 *
 * @return              The frequency in Hz.
 */
static ALWAYS_INLINE uint32_t chibios_sb_get_frequency(void) {

  __chibios_sb_svc0r(SB_FASTC_GET_FREQUENCY);
  return r0;
}

/**
 * @brief   Sets the host alarm, it is delivered on @p SB_VRQ_ALARM.
 * @note    A previously set alarm is reset first.
 *
 * @param[in] interval  alarm interval in host system time units, not zero
 * @param[in] reload    @p true for a periodic alarm
 * @return              The operation result, zero on success.
 */
static ALWAYS_INLINE int32_t chibios_sb_set_alarm(uint32_t interval,
                                                  bool reload) {

  __chibios_sb_svc2r(SB_FASTC_VRQ_SET_ALARM, interval, reload);
  return (int32_t)r0;
}

/**
 * @brief   Resets the host alarm.
 */
static ALWAYS_INLINE void chibios_sb_reset_alarm(void) {

  __chibios_sb_svc0(SB_FASTC_VRQ_RESET_ALARM);
}

/**
 * @brief   Suspends the whole sandbox in the host.
 * @note    The Zephyr scheduler does not run meanwhile, VRQs triggered
 *          during the sleep are served on return.
 *
 * @param[in] interval  sleep interval in host system time units
 */
static ALWAYS_INLINE void chibios_sb_sleep(uint32_t interval) {

  __chibios_sb_svc1r(SB_SYSC_SLEEP, interval);
  (void)r0;
}

/**
 * @brief   Writes on a sandbox file descriptor.
 *
 * @param[in] fd        file descriptor
 * @param[in] buf       data buffer
 * @param[in] n         number of bytes
 * @return              The number of bytes written or a negative error.
 */
static ALWAYS_INLINE int32_t chibios_sb_write(int fd, const void *buf,
                                              size_t n) {

  __chibios_sb_svc4r(SB_SYSC_POSIX, SB_POSIX_WRITE, fd, buf, n);
  return (int32_t)r0;
}

/**
 * @brief   Terminates the sandbox.
 *
 * @param[in] code      exit code reported to the host
 */
static ALWAYS_INLINE FUNC_NORETURN void chibios_sb_exit(uint32_t code) {

  __chibios_sb_svc1r(SB_SYSC_EXIT, code);
  (void)r0;
  CODE_UNREACHABLE;
}

/**
 * @brief   Writes the pads of a VIO GPIO unit.
 *
 * @param[in] unit      VIO GPIO unit
 * @param[in] bits      pads values
 */
static ALWAYS_INLINE void chibios_sb_vgpio_write(uint32_t unit,
                                                 uint32_t bits) {

  __chibios_sb_svc2r(SB_FASTC_VIO_VGPIO, VIO_CALL(SB_VGPIO_WRITE, unit),
                     bits);
  (void)r0;
}

/**
 * @brief   Toggles pads of a VIO GPIO unit.
 *
 * @param[in] unit      VIO GPIO unit
 * @param[in] bits      mask of the pads to be toggled
 */
static ALWAYS_INLINE void chibios_sb_vgpio_toggle(uint32_t unit,
                                                  uint32_t bits) {

  __chibios_sb_svc2r(SB_FASTC_VIO_VGPIO, VIO_CALL(SB_VGPIO_TOGGLE, unit),
                     bits);
  (void)r0;
}
#endif /* !defined(_ASMLANGUAGE) */

#endif /* ZEPHYR_INCLUDE_ARCH_CHIBIOS_SB_SB_H_ */

/** @} */
