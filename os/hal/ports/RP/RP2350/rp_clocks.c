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
 * @file    RP2350/rp_clocks.c
 * @brief   RP2350 clock driver source.
 * @note    See RP2350 Datasheet 8 Clocks
 *
 * @addtogroup RP_CLOCKS
 * @{
 */

#include "hal.h"
#include "rp_clocks.h"

/*===========================================================================*/
/* Driver local definitions.                                                 */
/*===========================================================================*/

/**
 * @brief   Estimated ROSC frequency for early timing.
 * @note    RP2350 ROSC varies 4.6-19.6 MHz, we assume ~6 MHz.
 *          This gives roughly +/-60% accuracy which is acceptable for
 *          safety timeouts during early clock initialization.
 */
#define RP_ROSC_ASSUMED_HZ      6000000U

#if RP_CLOCK_DYNAMIC == TRUE
#define RAMFUNC __attribute__((noinline, section(".ramtext")))
#endif

/*===========================================================================*/
/* Driver exported variables.                                                */
/*===========================================================================*/

#if (RP_CLOCK_DYNAMIC == TRUE) || defined(__DOXYGEN__)
/**
 * @brief   The clock configuration the system boots with.
 */
const halclkcfg_t hal_clkcfg_default = {
  .pll_sys_refdiv   = RP_PLL_SYS_REFDIV,
  .pll_sys_vco_freq = RP_PLL_SYS_VCO_FREQ,
  .pll_sys_postdiv1 = RP_PLL_SYS_POSTDIV1,
  .pll_sys_postdiv2 = RP_PLL_SYS_POSTDIV2,
  .qmi_clkdiv       = 0U
};

/**
 * @brief   A reduced-frequency configuration, 96 MHz.
 * @note    Assumes the default 12 MHz crystal; rejected by validation
 *          on configurations where the VCO settings do not divide.
 */
const halclkcfg_t hal_clkcfg_low = {
  .pll_sys_refdiv   = RP_PLL_SYS_REFDIV,
  .pll_sys_vco_freq = 768000000U,
  .pll_sys_postdiv1 = 4U,
  .pll_sys_postdiv2 = 2U,
  .qmi_clkdiv       = 0U
};
#endif /* RP_CLOCK_DYNAMIC == TRUE */

/*===========================================================================*/
/* Driver local variables and types.                                         */
/*===========================================================================*/

#if (RP_CLOCK_DYNAMIC == TRUE) || defined(__DOXYGEN__)
/**
 * @brief   Current clock point frequencies.
 * @details Zero-initialized in BSS; @p rp_clock_get_hz() serves the
 *          compile-time constants until @p rp_clock_init() populates
 *          this table.
 */
static uint32_t rp_clock_points[RP_CLK_COUNT];
#endif

/*===========================================================================*/
/* Driver local functions.                                                   */
/*===========================================================================*/

/**
 * @brief   Safely programs and starts a tick generator.
 * @note    The CYCLES register must not be rewritten while the generator
 *          is running, the counter is only reloaded when it reaches zero
 *          so a live rewrite can produce one wrong-length tick period.
 *          Disable the generator and wait until it reports not running
 *          before reprogramming it.
 *
 * @param[in] index     tick generator index (TICKS_xxx)
 * @param[in] cycles    clk_ref cycles per tick
 */
static void rp_tick_start(uint32_t index, uint32_t cycles) {

  osalDbgAssert(index <= TICKS_RISCV, "invalid tick generator index");

  TICKS->TICK[index].CTRL = 0U;
  while ((TICKS->TICK[index].CTRL & TICKS_CTRL_RUNNING) != 0U) {
    /* Waiting for the tick generator to stop */
  }
  TICKS->TICK[index].CYCLES = cycles;
  TICKS->TICK[index].CTRL = TICKS_CTRL_ENABLE;
}

/*===========================================================================*/
/* Driver exported functions.                                                */
/*===========================================================================*/

/**
 * @brief   Initializes all clocks.
 * @note    Most of this is derived from the RP2350 datasheet which directly
 *          references suggested code from the Pico SDK which is Copyright 
 *          2020 Raspberry Pi (Trading) Ltd and licensed under the
 *          BSD-3-Clause license. We always start with ROSC and then switch
 *          to XOSC.
 * @note    See RP2350 Datasheet 8.1.3.1 Clock Instances (Table 541)
 */
void rp_clock_init(void) {
  uint32_t cycles;

  /* Start early tick generator for safety module timeouts. */
  rp_peripheral_unreset(RESETS_ALLREG_TIMER0);

  /* Configure tick generator for ~1 us ticks. */
  rp_tick_start(TICKS_TIMER0, RP_ROSC_ASSUMED_HZ / 1000000U);

  /* Clear clock resus that may be in an unknown state */
  CLOCKS->RESUS.CTRL = 0U;

  rp_xosc_init();

  /* Switch clk_sys and clk_ref to safe sources */
  CLOCKS->CLR.CLK[RP_CLK_SYS].CTRL = CLOCKS_CLK_SYS_CTRL_SRC_Msk;
  while ((CLOCKS->CLK[RP_CLK_SYS].SELECTED & 1U) == 0U) {
    /* Wait for clk_sys to switch to clk_ref */
  }
  CLOCKS->CLR.CLK[RP_CLK_REF].CTRL = CLOCKS_CLK_REF_CTRL_SRC_Msk;
  while ((CLOCKS->CLK[RP_CLK_REF].SELECTED & 1U) == 0U) {
    /* Wait for clk_ref to switch to ROSC */
  }

  /* Initialize PLL_SYS: 12 MHz * 125 / 5 / 2 = 150 MHz. */
  rp_pll_init(PLL_SYS, RP_PLL_SYS_REFDIV, RP_PLL_SYS_VCO_FREQ,
              RP_PLL_SYS_POSTDIV1, RP_PLL_SYS_POSTDIV2);

  /* Initialize PLL_USB: 12 MHz * 100 / 5 / 5 = 48 MHz. */
  rp_pll_init(PLL_USB, RP_PLL_USB_REFDIV, RP_PLL_USB_VCO_FREQ,
              RP_PLL_USB_POSTDIV1, RP_PLL_USB_POSTDIV2);

  /* CLK_REF = XOSC = 12 MHz */
  {
    uint32_t src = CLOCKS_CLK_REF_CTRL_SRC_XOSC >> CLOCKS_CLK_REF_CTRL_SRC_Pos;
    CLOCKS->CLK[RP_CLK_REF].DIV = 1U << 16;
    CLOCKS->XOR.CLK[RP_CLK_REF].CTRL =
        (CLOCKS->CLK[RP_CLK_REF].CTRL ^ (src << CLOCKS_CLK_REF_CTRL_SRC_Pos)) &
        CLOCKS_CLK_REF_CTRL_SRC_Msk;
    while ((CLOCKS->CLK[RP_CLK_REF].SELECTED & (1U << src)) == 0U) {
      /* Wait for switch to XOSC */
    }
  }

  /* CLK_SYS = PLL_SYS = 150 MHz */
  CLOCKS->CLR.CLK[RP_CLK_SYS].CTRL = CLOCKS_CLK_SYS_CTRL_SRC_Msk;
  while ((CLOCKS->CLK[RP_CLK_SYS].SELECTED & 1U) == 0U) {
    /* Wait for switch to clk_ref */
  }
  CLOCKS->XOR.CLK[RP_CLK_SYS].CTRL =
      (CLOCKS->CLK[RP_CLK_SYS].CTRL ^ CLOCKS_CLK_SYS_CTRL_AUXSRC_PLL_SYS) &
      CLOCKS_CLK_SYS_CTRL_AUXSRC_Msk;
  CLOCKS->SET.CLK[RP_CLK_SYS].CTRL = CLOCKS_CLK_SYS_CTRL_SRC_AUX;
  while ((CLOCKS->CLK[RP_CLK_SYS].SELECTED & 2U) == 0U) {
    /* Wait for switch to aux */
  }

  /* CLK_USB = PLL_USB = 48 MHz */
  CLOCKS->XOR.CLK[RP_CLK_USB].CTRL =
      (CLOCKS->CLK[RP_CLK_USB].CTRL ^ CLOCKS_CLK_USB_CTRL_AUXSRC_PLL_USB) &
      CLOCKS_CLK_USB_CTRL_AUXSRC_Msk;
  CLOCKS->CLK[RP_CLK_USB].DIV = 1U << 16;
  CLOCKS->SET.CLK[RP_CLK_USB].CTRL = CLOCKS_CLK_PERI_CTRL_ENABLE;

  /* CLK_ADC = PLL_USB = 48 MHz */
  CLOCKS->XOR.CLK[RP_CLK_ADC].CTRL =
      (CLOCKS->CLK[RP_CLK_ADC].CTRL ^ CLOCKS_CLK_ADC_CTRL_AUXSRC_PLL_USB) &
      CLOCKS_CLK_ADC_CTRL_AUXSRC_Msk;
  CLOCKS->CLK[RP_CLK_ADC].DIV = 1U << 16;
  CLOCKS->SET.CLK[RP_CLK_ADC].CTRL = CLOCKS_CLK_PERI_CTRL_ENABLE;

  /* CLK_PERI = CLK_SYS = 150 MHz */
  CLOCKS->XOR.CLK[RP_CLK_PERI].CTRL =
      (CLOCKS->CLK[RP_CLK_PERI].CTRL ^ CLOCKS_CLK_PERI_CTRL_AUXSRC_SYS) &
      CLOCKS_CLK_PERI_CTRL_AUXSRC_Msk;
  CLOCKS->CLK[RP_CLK_PERI].DIV = 1U << 16;
  CLOCKS->SET.CLK[RP_CLK_PERI].CTRL = CLOCKS_CLK_PERI_CTRL_ENABLE;

  /* Calculate cycles for 1us tick based on clk_ref frequency, RP_XOSCCLK
     is checked at compile time to be an integer number of MHz. */
  cycles = RP_XOSCCLK / 1000000U;

  /* Start tick generators */
  for (uint32_t i = 0U; i < 6U; i++) {
    rp_tick_start(i, cycles);
  }

#if RP_CLOCK_DYNAMIC == TRUE
  /* Activating dynamic clock point queries, the boot values match the
     compile-time constants by construction. */
  rp_clock_points[RP_CLK_REF]  = RP_CLK_REF_FREQ;
  rp_clock_points[RP_CLK_PERI] = RP_CLK_PERI_FREQ;
  rp_clock_points[RP_CLK_USB]  = RP_CLK_USB_FREQ;
  rp_clock_points[RP_CLK_ADC]  = RP_CLK_ADC_FREQ;
  /* RP_CLK_SYS is written last, a non-zero value here switches
     rp_clock_get_hz() over to the table. */
  rp_clock_points[RP_CLK_SYS]  = RP_CLK_SYS_FREQ;
#endif
}

/**
 * @brief   Returns the frequency of a clock in Hz.
 * @note    Uses compile-time constants so this function is safe to call
 *          before BSS/DATA initialization.
 *
 * @param[in] clk_index     clock index (RP_CLK_xxx)
 * @return                  clock frequency in Hz
 */
uint32_t rp_clock_get_hz(uint32_t clk_index) {

  osalDbgAssert(clk_index < RP_CLK_COUNT, "invalid clock index");

#if RP_CLOCK_DYNAMIC == TRUE
  /* The table lives in BSS and stays zero until rp_clock_init() has
     populated it; falling through to the compile-time constants below
     preserves the documented pre-initialization callability, the
     constants are correct by definition until the first switch and the
     first switch cannot happen before initialization. */
  if (rp_clock_points[RP_CLK_SYS] != 0U) {
    return rp_clock_points[clk_index];
  }
#endif

  switch (clk_index) {
  case RP_CLK_REF:
    return RP_CLK_REF_FREQ;
  case RP_CLK_SYS:
    return RP_CLK_SYS_FREQ;
  case RP_CLK_PERI:
    return RP_CLK_PERI_FREQ;
  case RP_CLK_USB:
    return RP_CLK_USB_FREQ;
  case RP_CLK_ADC:
    return RP_CLK_ADC_FREQ;
  default:
    return 0U;
  }
}

#if (RP_CLOCK_DYNAMIC == TRUE) || defined(__DOXYGEN__)
/**
 * @brief   Checks a clock configuration for validity.
 * @details Applies at runtime the same constraints the port enforces at
 *          compile time on the static configuration, including the
 *          RP2350-E12 clk_sys/clk_usb ratio.
 *
 * @param[in] ccp       pointer to a @p halclkcfg_t structure
 * @return              @p true if the configuration is acceptable.
 */
static bool rp_clock_config_valid(const halclkcfg_t *ccp) {
  uint32_t ref_freq, fbdiv, pdiv, sys_freq;

  if ((ccp->pll_sys_refdiv < 1U) || (ccp->pll_sys_refdiv > 63U)) {
    return false;
  }
  if ((RP_XOSCCLK % ccp->pll_sys_refdiv) != 0U) {
    return false;
  }
  ref_freq = RP_XOSCCLK / ccp->pll_sys_refdiv;
  if (ref_freq < 5000000U) {
    return false;
  }
  if ((ccp->pll_sys_vco_freq < RP_PLL_VCO_MIN_FREQ) ||
      (ccp->pll_sys_vco_freq > RP_PLL_VCO_MAX_FREQ)) {
    return false;
  }
  if ((ccp->pll_sys_vco_freq % ref_freq) != 0U) {
    return false;
  }
  fbdiv = ccp->pll_sys_vco_freq / ref_freq;
  if ((fbdiv < 16U) || (fbdiv > 320U)) {
    return false;
  }
  if ((ccp->pll_sys_postdiv1 < 1U) || (ccp->pll_sys_postdiv1 > 7U) ||
      (ccp->pll_sys_postdiv2 < 1U) ||
      (ccp->pll_sys_postdiv2 > ccp->pll_sys_postdiv1)) {
    return false;
  }
  pdiv = ccp->pll_sys_postdiv1 * ccp->pll_sys_postdiv2;
  if ((ccp->pll_sys_vco_freq % pdiv) != 0U) {
    return false;
  }
  sys_freq = ccp->pll_sys_vco_freq / pdiv;

  /* No overclocking support, the port is validated up to the rated
     system frequency only. */
  if (sys_freq > RP_PLL_SYS_CLK) {
    return false;
  }

  /* RP2350-E12: reliable USB operation requires clk_sys >= 1.1 *
     clk_usb; clk_usb stays at its fixed frequency across switches. The
     comparison is done in 64 bits, this is runtime arithmetic. */
  if (((uint64_t)sys_freq * 10ULL) < ((uint64_t)RP_CLK_USB_FREQ * 11ULL)) {
    return false;
  }

  if (ccp->qmi_clkdiv > 255U) {
    return false;
  }

  return true;
}

/**
 * @brief   Reprograms the QMI flash clock divider.
 * @details Runs from RAM so no XIP fetch from this core is in flight
 *          while the timing register changes.
 * @note    This function MUST be in RAM.
 *
 * @param[in] clkdiv    new CLKDIV value, 1..255
 */
RAMFUNC static void rp_clock_set_qmi_clkdiv(uint32_t clkdiv) {

  QMI->M0_TIMING = (QMI->M0_TIMING & ~QMI_TIMING_CLKDIV_Msk) |
                   QMI_TIMING_CLKDIV(clkdiv);
  (void)QMI->M0_TIMING;
  __DSB();
  __ISB();
}

/**
 * @brief   Switches to a different clock configuration.
 * @details The switch keeps every clock consumer within specification
 *          at all times: the flash divider is first widened to a value
 *          safe at both the current and the target frequency, clk_sys
 *          (and clk_peri with it) is parked on clk_ref through the
 *          glitchless mux while PLL_SYS relocks, then the final flash
 *          divider is applied. clk_ref, clk_usb and clk_adc are not
 *          touched, so kernel time (TIMER0, fed from clk_ref) and the
 *          48 MHz peripherals are unaffected.
 * @note    Running peripheral drivers whose bit rates derive from
 *          clk_sys/clk_peri keep their old divider settings; the
 *          application must restart them after a switch, they then
 *          recompute from the updated clock points.
 * @note    On SMP configurations the other core keeps executing during
 *          the switch (timing stays in specification throughout); it
 *          slows to the parked frequency while PLL_SYS relocks.
 *
 * @param[in] ccp       pointer to a @p halclkcfg_t structure
 * @return              The operation status.
 * @retval false        if the switch operation succeeded.
 * @retval true         if the switch operation failed.
 *
 * @special
 */
bool hal_lld_clock_switch_mode(const halclkcfg_t *ccp) {
  uint32_t sys_freq, qmi_old, qmi_new, qmi_safe;
  syssts_t sts;

  osalDbgCheck(ccp != NULL);

  if (!rp_clock_config_valid(ccp)) {
    return true;
  }
  sys_freq = ccp->pll_sys_vco_freq /
             (ccp->pll_sys_postdiv1 * ccp->pll_sys_postdiv2);

  qmi_old  = (QMI->M0_TIMING & QMI_TIMING_CLKDIV_Msk) >>
             QMI_TIMING_CLKDIV_Pos;
  qmi_new  = (ccp->qmi_clkdiv != 0U) ? ccp->qmi_clkdiv : qmi_old;
  qmi_safe = (qmi_new > qmi_old) ? qmi_new : qmi_old;

  sts = osalSysGetStatusAndLockX();

  /* Flash divider safe at both the current and the target frequency
     before anything changes. */
  rp_clock_set_qmi_clkdiv(qmi_safe);

  /* Parking clk_sys on clk_ref through the glitchless mux; execution
     continues from flash at the reference frequency. */
  CLOCKS->CLR.CLK[RP_CLK_SYS].CTRL = CLOCKS_CLK_SYS_CTRL_SRC_Msk;
  while ((CLOCKS->CLK[RP_CLK_SYS].SELECTED & 1U) == 0U) {
    /* Waiting for clk_sys to run from clk_ref. */
  }

  /* Reprogramming PLL_SYS while nothing runs from it. */
  rp_pll_init(PLL_SYS, ccp->pll_sys_refdiv, ccp->pll_sys_vco_freq,
              ccp->pll_sys_postdiv1, ccp->pll_sys_postdiv2);

  /* Back onto the PLL through the glitchless mux. */
  CLOCKS->XOR.CLK[RP_CLK_SYS].CTRL =
      (CLOCKS->CLK[RP_CLK_SYS].CTRL ^ CLOCKS_CLK_SYS_CTRL_AUXSRC_PLL_SYS) &
      CLOCKS_CLK_SYS_CTRL_AUXSRC_Msk;
  CLOCKS->SET.CLK[RP_CLK_SYS].CTRL = CLOCKS_CLK_SYS_CTRL_SRC_AUX;
  while ((CLOCKS->CLK[RP_CLK_SYS].SELECTED & 2U) == 0U) {
    /* Waiting for clk_sys to run from PLL_SYS. */
  }

  /* Final flash divider for the new frequency. */
  rp_clock_set_qmi_clkdiv(qmi_new);

  /* Publishing the new frequencies; clk_peri follows clk_sys. */
  rp_clock_points[RP_CLK_SYS]  = sys_freq;
  rp_clock_points[RP_CLK_PERI] = sys_freq;
  SystemCoreClock = sys_freq;

  osalSysRestoreStatusX(sts);

  return false;
}
#endif /* RP_CLOCK_DYNAMIC == TRUE */

/** @} */
