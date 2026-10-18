This directory contains the ChibiOS bindings for the FreeRTOS kernel:
https://www.freertos.org

The bindings run FreeRTOS as a ChibiOS sandbox guest. The kernel and the
application run unprivileged inside a sandbox started by a ChibiOS SB host,
interrupts are replaced by the sandbox Virtual IRQs (VRQs) and peripherals
are accessed through the sandbox Virtual I/O (VIO).

Layout
------

- `freertos.mk`
  Kernel source list and include paths. The heap implementation is selected
  by `USE_FREERTOS_HEAP`: 1 to 5, default 4, or `none` for static
  allocation only.

- `portable/GCC/ARMvx-M-SB/`
  Port for ARMv7-M and ARMv8-M Mainline sandboxes, GCC only:
  - `port.mk`: port files, it also adds the common sandbox VRQ entry code.
  - `port.c`, `portasm.S`, `portmacro.h`: the port.
  - `porthooks.c`: default application hooks.

Usage
-----

1. Unpack `ext/FreeRTOS-Kernel-11.3.1.7z` under `ext/`, the kernel is placed
   in `ext/FreeRTOS-Kernel`.
2. Start from a sandbox guest makefile, for example the one of
   `demos/various/SB-CLIENT-FREERTOS`, including:
   - `$(CHIBIOS)/os/common/startup/ARMCMx-SB/compilers/GCC/mk/startup.mk`
   - `$(CHIBIOS)/os/various/freertos_bindings/freertos.mk`
   - `$(CHIBIOS)/os/various/freertos_bindings/portable/GCC/ARMvx-M-SB/port.mk`
   - `$(CHIBIOS)/os/sb/user/sbuser.mk`
3. Provide `FreeRTOSConfig.h` in the configuration directory, the demo one
   is a starting point.
4. Create the tasks and start the scheduler from `main()`.

Configuration
-------------

- `configUSE_TICKLESS_IDLE` must be 0.
- `configUSE_PORT_OPTIMISED_TASK_SELECTION` defaults to 1, it requires
  `configMAX_PRIORITIES` not greater than 32.
- `configTICK_RATE_HZ` should divide the host system time frequency
  returned by `sbGetFrequency()`, the tick period is rounded down to the
  host time resolution.
- `configUSE_IDLE_HOOK` should be 1, the default idle hook releases the CPU
  to the host until a VRQ is pending. Without it the idle task consumes all
  the CPU time the host assigns to the sandbox.
- `configASSERT(x)` should invoke `vAssertCalled(__FILE__, __LINE__)` on
  failure.
- `configUSE_MALLOC_FAILED_HOOK` and `configCHECK_FOR_STACK_OVERFLOW` are
  recommended, the default hooks terminate the sandbox.
- VRQ handlers run on the stack of the interrupted task, there is no
  separate interrupt stack. Task stacks must accommodate the deepest VRQ
  handler.

Default hooks
-------------

The port provides weak default hooks, they are used when enabled in
`FreeRTOSConfig.h` and not redefined by the application:

- `vApplicationIdleHook()`: waits for a VRQ using `vPortWaitForVRQ()`.
- `vAssertCalled()`: terminates the sandbox with exit code `0xA5nnnnnn`
  (`portSB_EXIT_ASSERT`), `nnnnnn` is the lower 16 bits of the line number.
- `vApplicationStackOverflowHook()`: terminates the sandbox with exit code
  `0xA6000000` (`portSB_EXIT_STACK_OVERFLOW`).
- `vApplicationMallocFailedHook()`: terminates the sandbox with exit code
  `0xA7000000` (`portSB_EXIT_MALLOC_FAILED`).

Scheduler and tick
------------------

- Context switches are performed in user mode. A switch requested inside a
  critical zone or by a VRQ handler is deferred to the critical zone exit or
  to the VRQ epilogue.
- The tick is a periodic host alarm, set up by `vPortSetupTimerInterrupt()`
  and delivered on `portVRQ_TICK` (VRQ 0). Ticks are accounted from the
  host system time, the ticks elapsed while the sandbox was not running are
  recovered on the next tick VRQ.
- `vTaskEndScheduler()` stops the tick alarm and `vTaskStartScheduler()`
  returns to `main()` with VRQs disabled. The sandbox terminates when
  `main()` returns, the returned value is the exit code. It must be invoked
  from a task other than the timer task, the kernel deletes the timer task.

VRQs
----

- `portVRQ_HANDLER(n)` defines the handler of VRQ `n`, a decimal literal
  without suffixes or a macro expanding to one. Handlers run with VRQs
  disabled, they can use the "FromISR" API and `portYIELD_FROM_ISR()`.
  They receive the VRQ number in `ulVRQ` and the flags set by the host, for
  example by VIO drivers, in `ulFlags`.
- `vPortVRQEnable()`, `vPortVRQDisable()`: enable or disable a VRQ.
- `vPortVRQTrigger()`: triggers a VRQ on the guest itself.
- VRQ 0 is used by the port, the VRQs of VIO units are assigned by the host
  configuration.

Virtual I/O and sandbox services
--------------------------------

- `os/sb/user/sbuservio.h`: VIO wrappers, `vgpioXxx()`, `vuartXxx()`,
  `vspiXxx()`, `vi2cXxx()`, `vadcXxx()`, `vgptXxx()` and `vethXxx()`. Units,
  configurations and VRQs are assigned by the host.
- `os/sb/common/sbsysc.h`: VIO return codes, VRQ flags and events,
  `SB_VIO_RET_xxx`, `SB_VIO_FL_xxx`, `SB_VUART_EV_xxx` and others.
- `os/sb/user/sbuser.h`: sandbox services, for example the host system time
  and the POSIX-like file API used by the C library standard I/O.
- `demos/various/SB-CLIENT-FREERTOS/source/vserial.c`: example of buffered
  serial driver over a VIO UART unit, based on FreeRTOS stream buffers.

FPU
---

- The port supports guests built for the FPU (`USE_FPU=hard`), the host
  must be built with FPU support too, else FPU instructions fault. With the
  multi-target host demos `make USE_FPU=hard` builds the host and the
  sandbox images for the FPU.
- In FPU builds the task context includes S16-S31 and FPSCR, S0-S15 are
  saved in the VRQ frame by the host. All tasks can use the FPU.

Limitations
-----------

- Tickless idle is not supported, the guest receives a VRQ on each tick.
- Blocking sandbox services, for example `sbSleepMilliseconds()`,
  `sbMsgWait()` or blocking file operations, suspend the whole guest, not
  just the calling task.
- With a host system time narrower than 32 bits, the ticks elapsed across a
  host system time wrap are lost.
