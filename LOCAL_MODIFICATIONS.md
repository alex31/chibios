# Local Modifications Report: ChibiOS 21.11.x
**Branch:** `stable_21.11.x_with_ext_and_local_fixes_mdma_fixes_sio_bsio_fixes_and_proposals`
**Base:** `stable_21.11.x` (Sub2Git Mirror)

This document explains the differences between the clean upstream branch and our local project branch.

---

## 🟢 Upstreamable Candidates
*These changes are generic fixes or standard feature completions that could be proposed to the official ChibiOS repository.*

### 1. STM32H7 FDCAN Registry Fixes
*   **File:** `os/hal/ports/STM32/STM32H7xx/stm32_registry.h`
*   **Change:** Added proper shared memory sizing and buffer/filter counts for FDCAN.
*   **Rationale:** Upstream often leaves these as defaults. These definitions are required for correct FDCAN initialization on H7 where memory must be partitioned between instances.

### 2. STM32H7 Backup Domain & BKPRAM Initialization
*   **File:** `os/hal/ports/STM32/STM32H7xx/hal_lld.c`
*   **Change:** Corrected the sequence for enabling the Backup Domain (DBP bit) and BKPRAM.
*   **Rationale:** Ensures that Backup RAM and RTC remain accessible and powered correctly after reset.

### 3. Infinite Loop "nop" for GCC Analyzer
*   **Files:**
    *   `os/common/startup/ARMCMx/compilers/GCC/crt1.c`
    *   `os/rt/src/chsys.c`
    *   `os/rt/src/chinstances.c`
    *   `os/hal/src/hal_safety.c`
*   **Change:** Added `asm volatile ("nop");` inside `while(true)` loops.
*   **Rationale:** Prevents recent GCC versions (with `-fanalyzer`) from triggering `analyzer-infinite-loop` errors.

### 4. Event Object Initialization Diagnostic Fix
*   **File:** `os/rt/include/chevents.h`
*   **Change:** Added `#pragma GCC diagnostic ignored "-Wanalyzer-allocation-size"` around `chEvtObjectInit`.
*   **Rationale:** Suppresses a false positive where the compiler thinks the pointer cast is an invalid allocation size.

### 5. GPT Interval Assertions
*   **File:** `os/hal/src/hal_gpt.c`
*   **Change:** Added `osalDbgAssert(interval > 1, ...)` to `gptStart` and `gptChangeInterval`.
*   **Rationale:** On many platforms, an interval of 1 or 0 can cause immediate interrupts or hardware hangs depending on the LLD implementation.

### 6. Test Suite Size Calculation
*   **File:** `os/test/src/ch_test.c`
*   **Change:** Cast linker symbols to `uint32_t` before subtraction.
*   **Rationale:** Prevents pointer arithmetic warnings/errors on some compiler configurations when calculating section sizes.

### 7. CAN Bus-Off Error Definition
*   **File:** `os/hal/include/hal_can.h`
*   **Change:** Defined `CAN_BUSOFF_ERROR`.
*   **Rationale:** Standardizes the error flag for CAN drivers that support bus-off detection.

---

## 🟡 Local Modifications / Proposals
*These should likely be kept as local changes because they are GCC-specific, project-specific, or change existing ChibiOS conventions.*

### 1. GCC Buffer Overflow Protection (Access Attributes)
*   **Files:**
    *   `os/common/portability/*/ccportab.h`
    *   `os/hal/osal/*/osal.h` and `os/hal/templates/osal/osal.h`
    *   HAL buffer APIs (ADC, buffers, channels/streams, DAC, flash, I2C,
        queues, SDC, SIO, SPIv1/v2, TRNG, UART, USB and WSPI)
    *   `os/rt/include/chthreads.h` for static thread working areas
*   **Change:** Added guarded `access(...)` and `nonnull_if_nonzero(...)`
    attributes to buffer APIs, plus a GCC 15+ `CC_NODISCARD_MSG(...)` macro
    carrying pedagogical messages. It is used by blocking I2C and SPIv2
    transfers and by timeout waits on semaphores, condition variables and
    events. A null
    buffer remains valid for a zero-length operation but is diagnosed when
    the associated size (or both dimensions) is nonzero. Some byte buffer
    pointers were changed to `void *` where the transfer unit is expressed in
    bytes. GCC-only API wrappers also reject constant arguments that violate
    existing ChibiOS preconditions: undersized or misaligned static thread
    working areas, invalid ADC/DAC depths, PWM channels beyond the hardware
    maximum, and invalid I2C addresses, zero mandatory transfer sizes or
    `TIME_IMMEDIATE` timeouts. I2C master addresses default to 7 bits and can
    be switched globally to 10 bits using
    `STM32_I2C_USE_10BIT_ADDRESSING` in `mcuconf.h`. Dynamic addresses are
    checked at run time; on STM32 I2Cv2/v3/v4, `i2cStart()` also verifies that
    `I2CConfig.cr2` uses the matching `I2C_CR2_ADD10` setting. On STM32
    I2Cv1, 10-bit mode rejects addresses below `0x80`, because that LLD would
    otherwise encode them as 7-bit transfers.
*   **Rationale:** **Highly valuable for students.** GCC can diagnose null
    buffers, undersized buffers, invalid thread working areas and ignored
    status values at compile time.
    `access` is enabled for GCC 10 and newer; `nonnull_if_nonzero` is enabled
    for GCC 15 and newer. Constant checks use `__builtin_constant_p` and the
    `error` function attribute, and are disabled for C++ and non-GCC
    compilers. Other compilers receive empty compatibility macros.

### 2. STM32G4 Enhanced Flash (EFL) Fast Programming
*   **Files:**
    *   `os/hal/ports/STM32/STM32G4xx/hal_efl_lld.c` / `.h`
*   **Change:** Added `efl_lld_fast_row_program` (256-byte chunks) and `HAL_EFL_SECTION_HOOK`.
*   **Rationale:** Significantly speeds up flash programming for bootloaders or high-speed data logging. The "Hook" allows placing the programming code in RAM to avoid stalling the CPU.

### 3. STM32H7 Non-Cacheable RAM Guard (FatFS)
*   **File:** `os/various/fatfs_bindings/fatfs_diskio.c`
*   **Change:** Added assertions to ensure FatFS buffers are located in the non-cacheable RAM section (`ram0nc`).
*   **Rationale:** Prevents DMA coherency issues on STM32H7. This is specific to how our H7 projects are partitioned and wouldn't fit all ChibiOS users.

### 4. SPI BDMA Domain3 Routing (H7)
*   **File:** `os/hal/ports/STM32/LLD/SPIv3/hal_spi_v2_lld.c`
*   **Change:** Added `dummyrx_d3` in `.ram4` section for BDMA transfers.
*   **Rationale:** Specific to H7 architecture where BDMA (used by SPI6) can only access Domain 3 memory.

### 5. Custom Syscalls and Linker Scripts
*   **Files:**
    *   `os/various/syscalls.c`
    *   `os/common/startup/ARMCMx/compilers/GCC/ld/STM32G491xx.ld`
*   **Rationale:** `syscalls.c` provides the bridge for `printf` to UART. `STM32G491xx.ld` is a custom-tailored linker script for that specific MCU part.

---
**Note:** All SIO (Serial I/O) changes from the old local repository were **intentionally removed** because the new `stable_21.11.x` upstream branch now contains superior native implementations of those fixes.
