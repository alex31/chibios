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
 * RP2350 XIP (Execute In Place) validation test.
 *
 * Thoroughly validates the QMI-based XIP path: register preservation
 * across flash operations, XIP readback correctness, cache behavior,
 * and resilience under repeated operation cycles.
 *
 * The RP2350 uses QMI register save/restore (M0_TIMING, M0_RFMT,
 * M0_RCMD) instead of boot2 re-execution.  The XIP cache is 16KB
 * with clean-then-invalidate maintenance.
 *
 * Uses the last 64KB block of flash to avoid overwriting firmware.
 * Single-core only (no SMP, no flash lockout).
 */

#include <string.h>

#include "ch.h"
#include "hal.h"
#include "chprintf.h"

#define TEST_BLOCK           (RP_FLASH_BLOCKS_COUNT - 1U)
#define SECTORS_PER_BLK      (RP_FLASH_BLOCK_64K_SIZE / RP_FLASH_SECTOR_SIZE)
#define FILL_PATTERN         0xA5U

#define LED_PIN              25U
#define UART_TX_PIN          0U
#define UART_RX_PIN          1U

static BaseSequentialStream *chp;
static unsigned pass_count;
static unsigned fail_count;

static uint8_t page_buf[RP_FLASH_PAGE_SIZE];
static BaseFlash *bfp;

/*===========================================================================*/
/* XIP snapshot.                                                             */
/*===========================================================================*/

typedef struct {
  uint32_t xip_ctrl;
  uint32_t qmi_direct_csr;
  uint32_t qmi_m0_timing;
  uint32_t qmi_m0_rfmt;
  uint32_t qmi_m0_rcmd;
  uint32_t qmi_m1_timing;
  uint32_t qmi_m1_rfmt;
  uint32_t qmi_m1_rcmd;
  uint32_t qmi_m1_wfmt;
  uint32_t qmi_m1_wcmd;
  uint32_t pads_qspi_sd[4];
} xip_snapshot_t;

static void xip_snapshot_capture(xip_snapshot_t *s) {

  s->xip_ctrl        = XIP_CTRL->CTRL;
  s->qmi_direct_csr  = QMI->DIRECT_CSR;
  s->qmi_m0_timing   = QMI->M0_TIMING;
  s->qmi_m0_rfmt     = QMI->M0_RFMT;
  s->qmi_m0_rcmd     = QMI->M0_RCMD;
  s->qmi_m1_timing   = QMI->M1_TIMING;
  s->qmi_m1_rfmt     = QMI->M1_RFMT;
  s->qmi_m1_rcmd     = QMI->M1_RCMD;
  s->qmi_m1_wfmt     = QMI->M1_WFMT;
  s->qmi_m1_wcmd     = QMI->M1_WCMD;
  s->pads_qspi_sd[0] = PADS_QSPI->GPIO_QSPI_SD0;
  s->pads_qspi_sd[1] = PADS_QSPI->GPIO_QSPI_SD1;
  s->pads_qspi_sd[2] = PADS_QSPI->GPIO_QSPI_SD2;
  s->pads_qspi_sd[3] = PADS_QSPI->GPIO_QSPI_SD3;
}

static bool xip_snapshot_compare(const xip_snapshot_t *a,
                                 const xip_snapshot_t *b) {

  return memcmp(a, b, sizeof(xip_snapshot_t)) == 0;
}

static void xip_snapshot_dump(const xip_snapshot_t *s) {

  chprintf(chp, "    XIP_CTRL    = 0x%08X\r\n", s->xip_ctrl);
  chprintf(chp, "    DIRECT_CSR  = 0x%08X\r\n", s->qmi_direct_csr);
  chprintf(chp, "    M0_TIMING   = 0x%08X\r\n", s->qmi_m0_timing);
  chprintf(chp, "    M0_RFMT     = 0x%08X\r\n", s->qmi_m0_rfmt);
  chprintf(chp, "    M0_RCMD     = 0x%08X\r\n", s->qmi_m0_rcmd);
  chprintf(chp, "    M1_TIMING   = 0x%08X\r\n", s->qmi_m1_timing);
  chprintf(chp, "    M1_RFMT     = 0x%08X\r\n", s->qmi_m1_rfmt);
  chprintf(chp, "    M1_RCMD     = 0x%08X\r\n", s->qmi_m1_rcmd);
  chprintf(chp, "    M1_WFMT     = 0x%08X\r\n", s->qmi_m1_wfmt);
  chprintf(chp, "    M1_WCMD     = 0x%08X\r\n", s->qmi_m1_wcmd);
  chprintf(chp, "    PADS SD0    = 0x%08X\r\n", s->pads_qspi_sd[0]);
  chprintf(chp, "    PADS SD1    = 0x%08X\r\n", s->pads_qspi_sd[1]);
  chprintf(chp, "    PADS SD2    = 0x%08X\r\n", s->pads_qspi_sd[2]);
  chprintf(chp, "    PADS SD3    = 0x%08X\r\n", s->pads_qspi_sd[3]);
}

/*===========================================================================*/
/* Test helpers.                                                             */
/*===========================================================================*/

static void report(const char *name, bool ok) {

  chprintf(chp, "  [%s] %s\r\n", ok ? "PASS" : "FAIL", name);
  if (ok) {
    pass_count++;
  }
  else {
    fail_count++;
  }
}

static void report_snapshot_mismatch(const xip_snapshot_t *before,
                                     const xip_snapshot_t *after) {

  chprintf(chp, "    Before:\r\n");
  xip_snapshot_dump(before);
  chprintf(chp, "    After:\r\n");
  xip_snapshot_dump(after);
}

static flash_error_t erase_sector(uint32_t block, unsigned sector_idx) {

  flash_sector_t s = block * SECTORS_PER_BLK + sector_idx;

  return flashStartEraseSector(bfp, s);
}

static flash_error_t erase_block(uint32_t block) {

  return efl_lld_start_erase_block(&EFLD1, RP_FLASH_CMD_BLOCK_ERASE_64K,
                                   RP_FLASH_BLOCK_64K_SIZE, block);
}

static flash_error_t program_page(uint32_t block, unsigned sector_idx,
                                  unsigned page_idx, uint8_t pattern) {

  flash_offset_t off = block * RP_FLASH_BLOCK_64K_SIZE +
                       sector_idx * RP_FLASH_SECTOR_SIZE +
                       page_idx * RP_FLASH_PAGE_SIZE;

  memset(page_buf, pattern, RP_FLASH_PAGE_SIZE);

  return flashProgram(bfp, off, RP_FLASH_PAGE_SIZE, page_buf);
}

/*===========================================================================*/
/* Individual tests.                                                         */
/*===========================================================================*/

/*
 * Test: Flash unique ID read — exercises a distinct XIP exit/enter
 * path (JEDEC read-unique-ID command).  Reads twice and verifies
 * consistency and that the ID is not blank.
 */
static bool test_unique_id(void) {
  uint8_t uid0[RP_FLASH_UNIQUE_ID_SIZE];
  uint8_t uid1[RP_FLASH_UNIQUE_ID_SIZE];
  flash_error_t err0, err1;
  bool all_zero = true;
  bool all_ff = true;
  unsigned i;

  err0 = efl_lld_read_unique_id(&EFLD1, uid0);
  err1 = efl_lld_read_unique_id(&EFLD1, uid1);

  /* On a failed read the buffers are not valid, skip the consistency
     and blank comparisons. */
  if ((err0 != FLASH_NO_ERROR) || (err1 != FLASH_NO_ERROR)) {
    chprintf(chp, "    UID read failed (%d, %d)\r\n", (int)err0, (int)err1);
    return false;
  }

  chprintf(chp, "    UID: ");
  for (i = 0U; i < RP_FLASH_UNIQUE_ID_SIZE; i++) {
    chprintf(chp, "%02X", uid0[i]);
    if (uid0[i] != 0U)
      all_zero = false;
    if (uid0[i] != 0xFFU)
      all_ff = false;
  }
  chprintf(chp, "\r\n");

  return (memcmp(uid0, uid1, sizeof(uid0)) == 0) && !all_zero && !all_ff;
}

/*
 * Test: QMI register preservation across sector erase.
 */
static bool test_qmi_regs_sector_erase(uint32_t block) {
  xip_snapshot_t before, after;

  xip_snapshot_capture(&before);
  if (erase_sector(block, 0U) != FLASH_NO_ERROR)
    return false;
  xip_snapshot_capture(&after);

  if (!xip_snapshot_compare(&before, &after)) {
    report_snapshot_mismatch(&before, &after);
    return false;
  }

  return true;
}

/*
 * Test: QMI register preservation across page program.
 */
static bool test_qmi_regs_page_program(uint32_t block) {
  xip_snapshot_t before, after;

  xip_snapshot_capture(&before);
  if (program_page(block, 0U, 0U, FILL_PATTERN) != FLASH_NO_ERROR)
    return false;
  xip_snapshot_capture(&after);

  if (!xip_snapshot_compare(&before, &after)) {
    report_snapshot_mismatch(&before, &after);
    return false;
  }

  return true;
}

/*
 * Test: QMI register preservation across block erase.
 */
static bool test_qmi_regs_block_erase(uint32_t block) {
  xip_snapshot_t before, after;

  xip_snapshot_capture(&before);
  if (erase_block(block) != FLASH_NO_ERROR)
    return false;
  xip_snapshot_capture(&after);

  if (!xip_snapshot_compare(&before, &after)) {
    report_snapshot_mismatch(&before, &after);
    return false;
  }

  return true;
}

/*
 * Test: XIP readback after page program.
 */
static bool test_xip_readback_program(uint32_t block) {
  flash_offset_t off = block * RP_FLASH_BLOCK_64K_SIZE;
  const uint8_t *xip = (const uint8_t *)(RP_FLASH_BASE + off);
  unsigned i;

  if (erase_sector(block, 0U) != FLASH_NO_ERROR)
    return false;

  if (program_page(block, 0U, 0U, FILL_PATTERN) != FLASH_NO_ERROR)
    return false;

  for (i = 0U; i < RP_FLASH_PAGE_SIZE; i++) {
    if (xip[i] != FILL_PATTERN)
      return false;
  }

  /* Bytes beyond the written page must still be erased. */
  if (xip[RP_FLASH_PAGE_SIZE] != 0xFFU)
    return false;

  return true;
}

/*
 * Test: XIP readback after sector erase (all 0xFF).
 */
static bool test_xip_readback_erase(uint32_t block) {
  flash_offset_t off = block * RP_FLASH_BLOCK_64K_SIZE +
                       1U * RP_FLASH_SECTOR_SIZE;
  const uint8_t *xip = (const uint8_t *)(RP_FLASH_BASE + off);
  unsigned i;

  /* Program a page in sector 1 first. */
  if (program_page(block, 1U, 0U, 0xBBU) != FLASH_NO_ERROR)
    return false;

  /* Erase the sector. */
  if (erase_sector(block, 1U) != FLASH_NO_ERROR)
    return false;

  /* All bytes must read 0xFF via XIP. */
  for (i = 0U; i < RP_FLASH_SECTOR_SIZE; i++) {
    if (xip[i] != 0xFFU)
      return false;
  }

  return true;
}

/*
 * Test: Firmware area integrity after XIP restore.
 *
 * Reads 256 bytes (64 words) from the start of flash twice, with
 * a flash operation in between.  Both reads go through cold cache
 * (each erase flushes the cache), so a cache-flush regression
 * would cause the second read to return stale or corrupted data.
 */
static bool test_firmware_readable(void) {
  const uint32_t *fw = (const uint32_t *)RP_FLASH_BASE;
  static uint32_t fw_snapshot[64];
  unsigned i;

  /* First flash operation — flushes XIP cache. */
  if (erase_sector(TEST_BLOCK, 2U) != FLASH_NO_ERROR)
    return false;

  /* First read of firmware area — cold cache after erase. */
  memcpy(fw_snapshot, fw, sizeof(fw_snapshot));

  /* Second flash operation — flushes XIP cache again. */
  if (erase_sector(TEST_BLOCK, 2U) != FLASH_NO_ERROR)
    return false;

  /* Second read — cold cache, must match first. */
  for (i = 0U; i < 64U; i++) {
    if (fw[i] != fw_snapshot[i]) {
      chprintf(chp, "    Firmware word %u: expected 0x%08X got 0x%08X\r\n",
               i, fw_snapshot[i], fw[i]);
      return false;
    }
  }

  return true;
}

/*
 * Test: Repeated erase/program/verify cycles to catch state
 * accumulation bugs.
 */
static bool test_repeated_cycles(uint32_t block) {
  unsigned cycle;

  for (cycle = 0U; cycle < 10U; cycle++) {
    uint8_t pattern = (uint8_t)(cycle * 17U + 0x11U);
    flash_offset_t off = block * RP_FLASH_BLOCK_64K_SIZE +
                         3U * RP_FLASH_SECTOR_SIZE;
    const uint8_t *xip = (const uint8_t *)(RP_FLASH_BASE + off);
    unsigned i;

    if (erase_sector(block, 3U) != FLASH_NO_ERROR)
      return false;

    if (program_page(block, 3U, 0U, pattern) != FLASH_NO_ERROR)
      return false;

    for (i = 0U; i < RP_FLASH_PAGE_SIZE; i++) {
      if (xip[i] != pattern) {
        chprintf(chp, "    Cycle %u: byte %u expected 0x%02X got 0x%02X\r\n",
                 cycle, i, pattern, xip[i]);
        return false;
      }
    }
  }

  return true;
}

/*
 * Test: Rapid alternating erase/program across different sectors
 * with no delays. Verifies XIP restoration is complete between
 * back-to-back operations.
 */
static bool test_rapid_alternating(uint32_t block) {
  unsigned i;

  for (i = 0U; i < 10U; i++) {
    unsigned sec_a = 4U + (i % 4U);
    unsigned sec_b = 4U + ((i + 1U) % 4U);
    uint8_t pattern = (uint8_t)(i + 0x30U);
    flash_offset_t off_a = block * RP_FLASH_BLOCK_64K_SIZE +
                           sec_a * RP_FLASH_SECTOR_SIZE;
    const uint8_t *xip_a = (const uint8_t *)(RP_FLASH_BASE + off_a);

    /* Erase sector A. */
    if (erase_sector(block, sec_a) != FLASH_NO_ERROR)
      return false;

    /* Immediately program sector B (no sleep). */
    if (erase_sector(block, sec_b) != FLASH_NO_ERROR)
      return false;
    if (program_page(block, sec_b, 0U, pattern) != FLASH_NO_ERROR)
      return false;

    /* Verify sector A is erased via XIP. */
    if (xip_a[0] != 0xFFU || xip_a[255] != 0xFFU)
      return false;

    /* Verify sector B has the pattern via XIP. */
    {
      flash_offset_t off_b = block * RP_FLASH_BLOCK_64K_SIZE +
                             sec_b * RP_FLASH_SECTOR_SIZE;
      const uint8_t *xip_b = (const uint8_t *)(RP_FLASH_BASE + off_b);

      if (xip_b[0] != pattern || xip_b[RP_FLASH_PAGE_SIZE - 1U] != pattern)
        return false;
    }
  }

  return true;
}

/*
 * Test: Full 64KB block write then verify via XIP.
 */
static bool test_full_block_write_verify(uint32_t block) {
  flash_offset_t base = block * RP_FLASH_BLOCK_64K_SIZE;
  const uint8_t *xip = (const uint8_t *)(RP_FLASH_BASE + base);
  unsigned page, i;

  /* Erase entire block. */
  if (erase_block(block) != FLASH_NO_ERROR)
    return false;

  /* Program all 256 pages with incrementing pattern. */
  for (page = 0U; page < (RP_FLASH_BLOCK_64K_SIZE / RP_FLASH_PAGE_SIZE); page++) {
    uint8_t pattern = (uint8_t)((page ^ 0x55U) & 0xFFU);

    memset(page_buf, pattern, RP_FLASH_PAGE_SIZE);
    if (flashProgram(bfp, base + page * RP_FLASH_PAGE_SIZE,
                     RP_FLASH_PAGE_SIZE, page_buf) != FLASH_NO_ERROR)
      return false;
  }

  /* Verify entire 64KB via XIP in one pass. */
  for (page = 0U; page < (RP_FLASH_BLOCK_64K_SIZE / RP_FLASH_PAGE_SIZE); page++) {
    uint8_t expected = (uint8_t)((page ^ 0x55U) & 0xFFU);

    for (i = 0U; i < RP_FLASH_PAGE_SIZE; i++) {
      if (xip[page * RP_FLASH_PAGE_SIZE + i] != expected) {
        chprintf(chp, "    Page %u byte %u: expected 0x%02X got 0x%02X\r\n",
                 page, i, expected,
                 xip[page * RP_FLASH_PAGE_SIZE + i]);
        return false;
      }
    }
  }

  return true;
}

/*
 * Test: ATRANS-aware program offset translation.
 *
 * Picks the first free 4 MiB ATRANS window at or above the flash top
 * and derives the physical address of the sacrificial last sector by
 * translating its logical offset through the current mapping of the
 * window containing it (no identity-mapping assumption).  The free
 * window is aliased directly onto that physical sector (BASE is in
 * 4 KiB units, SIZE covers one sector), a pattern is programmed
 * through alias offset 0 and verified through the normal logical
 * last-sector offset.  Also verifies that an offset outside the
 * mapped window size (SIZE=0) is rejected.  Only touches the
 * sacrificial last sector; the alias window is saved and restored
 * and cleanup goes through the standard mapping.
 */
static bool test_atrans_translation(void) {
  flash_offset_t last_off = TEST_BLOCK * RP_FLASH_BLOCK_64K_SIZE +
                            (SECTORS_PER_BLK - 1U) * RP_FLASH_SECTOR_SIZE;
  uint32_t idx = (RP_FLASH_SIZE + 0x3FFFFFU) >> 22;
  uint32_t win = ((uint32_t)last_off >> 22) & 7U;
  uint32_t at, phys_last, alias_off, atrans_save;
  flash_error_t err;
  syssts_t sts;
  unsigned i;
  bool ok = true;

  /* A flash filling the whole 32 MiB XIP space leaves no free alias
     window; nothing can be validated without touching live mappings. */
  if (idx > 7U) {
    chprintf(chp, "    No free ATRANS window above flash top, skipped\r\n");
    return true;
  }

  /* Physical address of the last sector, translated through the
     current mapping of the window containing it. */
  at          = QMI->ATRANS[win];
  phys_last   = (((at & QMI_ATRANS_BASE_Msk) >> QMI_ATRANS_BASE_Pos) << 12) +
                ((uint32_t)last_off & 0x3FFFFFU);
  alias_off   = idx * 0x400000U;
  atrans_save = QMI->ATRANS[idx];

  chprintf(chp, "    Alias window %u -> phys 0x%08X\r\n", idx, phys_last);

  /* Start from an erased last sector (normal logical offset). */
  if (erase_sector(TEST_BLOCK, SECTORS_PER_BLK - 1U) != FLASH_NO_ERROR)
    return false;

  /* Alias window offset 0 exactly onto the physical last sector,
     mapping a single sector. */
  QMI->ATRANS[idx] = ((phys_last >> 12) << QMI_ATRANS_BASE_Pos) |
                     ((RP_FLASH_SECTOR_SIZE >> 12) << QMI_ATRANS_SIZE_Pos);

  /* Program a pattern through the aliased offset.  The engine-level
     API bounds offsets to the flash descriptor, so the per-chip entry
     point is called directly under the same syslock bracket the
     engine uses. */
  memset(page_buf, 0xC3, RP_FLASH_PAGE_SIZE);
  sts = osalSysGetStatusAndLockX();
  err = rp_efl_lld_program_page_full(&EFLD1, alias_off, page_buf,
                                     RP_FLASH_PAGE_SIZE);
  osalSysRestoreStatusX(sts);
  if (err != FLASH_NO_ERROR) {
    chprintf(chp, "    Aliased program failed (%d)\r\n", (int)err);
    ok = false;
  }

  /* Verify through the normal logical last-sector offset that the
     physical last sector changed. */
  if (ok &&
      (flashRead(bfp, last_off, RP_FLASH_PAGE_SIZE,
                 page_buf) != FLASH_NO_ERROR)) {
    ok = false;
  }
  if (ok) {
    for (i = 0U; i < RP_FLASH_PAGE_SIZE; i++) {
      if (page_buf[i] != 0xC3U) {
        chprintf(chp, "    Byte %u: expected 0xC3 got 0x%02X\r\n",
                 i, page_buf[i]);
        ok = false;
        break;
      }
    }
  }

  /* An offset beyond the mapped window size (SIZE=0) must be rejected
     while XIP is still enabled. */
  if (ok) {
    QMI->ATRANS[idx] = ((phys_last >> 12) << QMI_ATRANS_BASE_Pos) |
                       (0x0U << QMI_ATRANS_SIZE_Pos);
    sts = osalSysGetStatusAndLockX();
    err = rp_efl_lld_program_page_full(&EFLD1, alias_off, page_buf,
                                       RP_FLASH_PAGE_SIZE);
    osalSysRestoreStatusX(sts);
    if (err != FLASH_ERROR_HW_FAILURE) {
      chprintf(chp, "    Unmapped offset not rejected (%d)\r\n", (int)err);
      ok = false;
    }
  }

  /* Restore the saved ATRANS window. */
  QMI->ATRANS[idx] = atrans_save;

  /* Leave the sector erased, cleaned up through the standard mapping. */
  if (erase_sector(TEST_BLOCK, SECTORS_PER_BLK - 1U) != FLASH_NO_ERROR)
    ok = false;

  return ok;
}

/*===========================================================================*/
/* Blinker thread.                                                           */
/*===========================================================================*/

static THD_WORKING_AREA(waThread1, 256);
static THD_FUNCTION(Thread1, arg) {

  (void)arg;
  chRegSetThreadName("blinker");
  while (true) {
    palToggleLine(LED_PIN);
    chThdSleepMilliseconds(500);
  }
}

/*===========================================================================*/
/* Main.                                                                     */
/*===========================================================================*/

int main(void) {
  bool ok;

  halInit();
  chSysInit();

  /* LED. */
  palSetLineMode(LED_PIN, PAL_MODE_OUTPUT_PUSHPULL | PAL_RP_PAD_DRIVE12);

  /* UART on GPIO0/GPIO1. */
  palSetLineMode(UART_TX_PIN, PAL_MODE_ALTERNATE_UART);
  palSetLineMode(UART_RX_PIN, PAL_MODE_ALTERNATE_UART);
  sioStart(&SIOD0, NULL);
  chp = (BaseSequentialStream *)&SIOD0;

  /* Start EFL driver. */
  eflStart(&EFLD1, NULL);
  bfp = getBaseFlash(&EFLD1);

  /* Start blinker. */
  chThdCreateStatic(waThread1, sizeof(waThread1), NORMALPRIO, Thread1, NULL);

  /* Small delay to let UART settle. */
  chThdSleepMilliseconds(100);

  chprintf(chp, "\r\n");
  chprintf(chp, "========================================\r\n");
  chprintf(chp, "  RP2350 XIP Validation\r\n");
  chprintf(chp, "========================================\r\n");
  chprintf(chp, "  Flash size:  %u KB\r\n", RP_FLASH_SIZE / 1024U);
  chprintf(chp, "  Test block:  %u (offset 0x%08X)\r\n",
           TEST_BLOCK, TEST_BLOCK * RP_FLASH_BLOCK_64K_SIZE);
  chprintf(chp, "\r\n");

  /* Test 1: Pre-erase test block. */
  chprintf(chp, "  Pre-erasing test block...\r\n");
  ok = erase_block(TEST_BLOCK) == FLASH_NO_ERROR;
  report("Pre-erase test block", ok);
  if (!ok)
    goto done;

  /* Test 2: Unique ID read. */
  ok = test_unique_id();
  report("Flash unique ID consistent and not blank", ok);

  /* Test 3: QMI regs across sector erase. */
  ok = test_qmi_regs_sector_erase(TEST_BLOCK);
  report("QMI regs preserved across sector erase", ok);

  /* Test 4: Setup erase for page program test. */
  ok = erase_sector(TEST_BLOCK, 0U) == FLASH_NO_ERROR;
  report("Setup erase for page program test", ok);
  if (!ok)
    goto done;

  /* Test 5: QMI regs across page program. */
  ok = test_qmi_regs_page_program(TEST_BLOCK);
  report("QMI regs preserved across page program", ok);

  /* Test 6: QMI regs across block erase. */
  ok = test_qmi_regs_block_erase(TEST_BLOCK);
  report("QMI regs preserved across block erase", ok);

  /* Test 7: XIP readback after program. */
  ok = test_xip_readback_program(TEST_BLOCK);
  report("XIP readback correct after page program", ok);

  /* Test 8: XIP readback after erase. */
  ok = test_xip_readback_erase(TEST_BLOCK);
  report("XIP readback all 0xFF after sector erase", ok);

  /* Test 9: Firmware area integrity after XIP restore. */
  ok = test_firmware_readable();
  report("Firmware area consistent via XIP after flash ops", ok);

  /* Test 10: Repeated cycles. */
  chprintf(chp, "\r\n  Repeated erase/program/verify (10 cycles)...\r\n");
  ok = test_repeated_cycles(TEST_BLOCK);
  report("10 erase/program/verify cycles pass", ok);

  /* Test 11: Setup erase for rapid alternating. */
  chprintf(chp, "\r\n  Rapid alternating erase/program...\r\n");
  ok = erase_block(TEST_BLOCK) == FLASH_NO_ERROR;
  report("Setup erase for rapid alternating test", ok);
  if (!ok)
    goto done;

  /* Test 12: Rapid alternating. */
  ok = test_rapid_alternating(TEST_BLOCK);
  report("Rapid alternating ops with XIP verify", ok);

  /* Test 13: Full block write + verify. */
  chprintf(chp, "\r\n  Full 64KB write + XIP verify...\r\n");
  ok = test_full_block_write_verify(TEST_BLOCK);
  report("Full 64KB block write and XIP readback", ok);

  /* Test 14: ATRANS translation. */
  chprintf(chp, "\r\n  ATRANS translation...\r\n");
  ok = test_atrans_translation();
  report("atrans translation", ok);

  /* Cleanup. */
  chprintf(chp, "\r\n  Cleanup block erase...\r\n");
  erase_block(TEST_BLOCK);

done:
  chprintf(chp, "\r\n========================================\r\n");
  chprintf(chp, "  Results: %u pass, %u fail\r\n", pass_count, fail_count);
  if (fail_count == 0U) {
    chprintf(chp, "  ALL TESTS PASSED\r\n");
  }
  else {
    chprintf(chp, "  *** FAILURES DETECTED ***\r\n");
  }
  chprintf(chp, "========================================\r\n");

  while (true) {
    chThdSleepMilliseconds(1000);
  }

  return 0;
}
