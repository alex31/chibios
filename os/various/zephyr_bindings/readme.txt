This directory contains the ChibiOS bindings for the Zephyr kernel:
https://www.zephyrproject.org

The bindings run Zephyr as a ChibiOS sandbox guest. The kernel and the
application run unprivileged inside a sandbox started by a ChibiOS SB host,
interrupts are replaced by the sandbox Virtual IRQs (VRQs). Only the kernel
is supported for now, peripherals are not available as Zephyr devices.

The directory is a Zephyr module providing the "chibios_sb" architecture,
SoC and board, plus the system timer driver.

Requirements
------------

- A Zephyr tree with the external architecture support, CONFIG_ARCH_EXTERNAL.
  Zephyr finds out-of-tree architectures through ARCH_ROOT but some headers
  only dispatch to the in-tree ones. Until an equivalent change is
  available upstream, apply `patches/zephyr-arch-external.patch` to the
  Zephyr tree, the patch header describes the changes and the base
  revision.
- The west tool and the Zephyr Python requirements.
- An ARM GCC toolchain, the ChibiStudio one can be used through the
  "gnuarmemb" Zephyr toolchain variant:
  ZEPHYR_TOOLCHAIN_VARIANT=gnuarmemb
  GNUARMEMB_TOOLCHAIN_PATH=<ChibiStudio>/tools/arm-none-eabi/<version>

Layout
------

- `zephyr/module.yml`
  Module description, the module is also the architecture, SoC and board
  root.

- `arch/chibios_sb/`
  Architecture code:
  - `core/sb_entry.S`: sandbox header, entry point, VRQ entry point and
    context switch.
  - `core/irq_manage.c`: VRQ enable/disable and VRQ dispatching.
  - `core/thread.c`: threads initial context.
  - `core/cpu_idle.c`: idle, the CPU is released to the host.
  - `core/fatal.c`: system halt, it terminates the sandbox.
  - `core/printk.c`: printk() output on the sandbox standard output.
  - `core/irq_offload.c`: irq_offload() on a self-triggered VRQ.
  - `core/prep_c.c`: C environment initialization.

- `include/zephyr/arch/chibios_sb/`
  Architecture public headers and linker script. `sb.h` contains the
  sandbox fastcall and syscall wrappers, the codes are the ones shared by
  all the sandbox guests in `os/sb/common/sbsysc.h`.

- `include/zephyr/arch/external/`
  Headers included by the Zephyr dispatch headers when
  CONFIG_ARCH_EXTERNAL is selected, they forward to the architecture ones.

- `soc/chibios/sb/`, `boards/chibios/chibios_sb/`
  The "chibios_sb" SoC and board. The board does not define memory, the
  sandbox slot is defined by an application overlay providing the
  zephyr,flash and zephyr,sram chosen nodes, it must match the host
  configuration of the sandbox regions.

- `drivers/timer/chibios_sb_timer.c`
  System timer, based on the Zephyr generic timer core.

- `patches/zephyr-arch-external.patch`
  Zephyr external architecture support, see the requirements.

Usage
-----

See `demos/various/SB-CLIENT-ZEPHYR`, from a shell where the Zephyr
environment is active:

  west build -b chibios_sb <ChibiOS>/demos/various/SB-CLIENT-ZEPHYR
  west build -b chibios_sb <ChibiOS>/demos/various/SB-CLIENT-ZEPHYR \
    -- -DSB_SLOT=128k-081E0000-16k-20004000

The application CMakeLists.txt adds this directory to ZEPHYR_EXTRA_MODULES
and the slot overlay to EXTRA_DTC_OVERLAY_FILE. The output is
`build/zephyr/zephyr.elf`, it is loaded in the sandbox slot like the other
sandbox images.

Architecture
------------

- The sandbox header is at the start of the code region, the host starts
  the sandbox at `__start` with VRQs disabled. The kernel initialization
  runs on the interrupt stack, the startup stack, arguments and
  environment placed by the host at the top of the data region are not
  used.
- The IRQ lock is the VRQs global disable, `irq_lock()` and `irq_unlock()`
  are fastcalls. IRQ numbers are VRQ numbers, 0 to 31, priorities and
  flags are ignored. `IRQ_CONNECT()` places the handler in the software
  ISR table generated at build time.
- VRQ handlers run on the stack of the interrupted thread, there is no
  separate interrupt stack. Thread stacks, including the idle thread one,
  must accommodate the host VRQ frame, up to 108 bytes, and the deepest
  VRQ handler.
- Context switches are performed in user mode by a function call saving
  the callee-saved registers on the outgoing thread stack, the stack
  pointer is the thread switch handle (CONFIG_USE_SWITCH). A switch
  requested by a VRQ handler is performed at the end of the handler, the
  VRQ frame of the preempted thread is popped when it is switched in again.
- The idle thread releases the CPU to the host until a VRQ is pending.
- The system timer counter is the host system time, its frequency is read
  at run time and determines the cycle counter resolution. The timer is
  the host alarm on VRQ 0, the kernel is tickless. The ticks elapsed while
  the sandbox was not running are recovered from the counter. The tick
  rate, CONFIG_SYS_CLOCK_TICKS_PER_SEC, must not exceed the host system
  time frequency.
- A system halt, for example after a fatal error or a failed assertion,
  terminates the sandbox with exit code `0xA8000000` ORed with the reason.
- printk() writes on the sandbox standard output, buffered by line.
- irq_offload() uses VRQ 31, CONFIG_CHIBIOS_SB_IRQ_OFFLOAD_VRQ.

Limitations
-----------

- Soft-float only, FPU registers are not part of the thread context.
- No user mode, no SMP, no Zephyr devices.
- Blocking sandbox services, for example host sleeps or blocking file
  operations, suspend the whole guest, not just the calling thread.
- With VRQs disabled, irq_offload() routines run when VRQs are enabled
  again instead of synchronously.
