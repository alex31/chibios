# RT-SB-SWITCHED-XSHELL-FREERTOS-MULTI

Multi-target XHAL sandbox host running two static sandboxes with switched MPU
regions:

- SB1 runs the ChibiOS/RT XSHELL client (`demos/various/SB-CLIENT-RT-XHAL-XSHELL`).
  It owns the console UART through VIO UART unit 0, VRQ 8.
- SB2 runs the FreeRTOS client (`demos/various/SB-CLIENT-FREERTOS`). It owns
  the board LED through VIO GPIO unit 0. Its standard input, output and error
  are connected to `/dev/null`.

The host restarts a sandbox when it terminates and reports the exit code on
its own serial port.

## FreeRTOS sandbox behavior

At start the FreeRTOS client runs a self-test of the port for about 2.2
seconds, the LED is on during the test. After the test:

- LED blinking at 1 Hz: all checks passed.
- LED blinking at 5 Hz: at least one check failed.

Fatal conditions terminate the sandbox, the host reports the exit code and
restarts it, so the LED goes back to steady on while the self-test runs again:

- `0xA5nnnnnn`: `configASSERT()` failed, `nnnnnn` is the line number.
- `0xA6000000`: task stack overflow.
- `0xA7000000`: FreeRTOS heap allocation failure.

The RT XSHELL client also toggles VIO GPIO unit 0 from its blinker thread,
SB1 has no GPIO units assigned so those operations are ignored by the host.

## Build

From this directory:

```sh
make -j8
```

For each target the sandbox images are built first, from their own projects,
using the makefiles matching the target's sandbox slots, then the host. The
host output is `build/<target>/ch.elf`, the sandbox images are the `sb.elf`
files under the client projects' `build/<slot>/` directories.

`make clean` cleans the host only. The sandbox images are cleaned from their
own projects.

## STM32G474RE Nucleo-64

- Console (SB1): ST-LINK virtual COM port, LPUART1 on PA2/PA3, 38400 baud,
  8N1.
- Host messages: USART1 on PA9/PA10 (Arduino D8/D2), 38400 baud, 8N1. A
  separate USB-serial adapter is required.
- LED (SB2): the board's green LED on PA5.
- Memory map, see `ld/stm32g474re_nucleo64.ld`:

  | Region | Address      | Size  |
  |--------|--------------|-------|
  | Host   | `0x08000000` | 256k  |
  | SB1    | `0x08040000` | 128k  |
  | SB2    | `0x08060000` | 128k  |
  | Host   | `0x20000000` | 96k   |
  | SB1    | `0x20018000` | 16k   |
  | SB2    | `0x2001C000` | 16k   |

- Sandbox images:
  - `demos/various/SB-CLIENT-RT-XHAL-XSHELL/build/128k-08040000-16k-20018000/sb.elf`
  - `demos/various/SB-CLIENT-FREERTOS/build/128k-08060000-16k-2001C000/sb.elf`

All three images must be flashed, for example using OpenOCD:

```sh
openocd -f interface/stlink.cfg -f target/stm32g4x.cfg \
  -c "program build/stm32g474re_nucleo64/ch.elf verify" \
  -c "program ../../various/SB-CLIENT-RT-XHAL-XSHELL/build/128k-08040000-16k-20018000/sb.elf verify" \
  -c "program ../../various/SB-CLIENT-FREERTOS/build/128k-08060000-16k-2001C000/sb.elf verify reset exit"
```

From a GDB session the sandbox images can be loaded after the host image
using `load <path to sb.elf>`.

## Adding targets

Add `make/<target>.make` and a self-contained `cfg/<target>/` with the RT,
XHAL, MCU, sandbox, VIO and VFS configurations plus `portab.c/h`. Define the
host serial port, the SB1 console SIO and the LED port and pad in `portab.h`.
Provide a linker script defining the host and the two sandbox regions, then
select the sandbox images matching those regions in the `sandboxes` rule of
the target makefile. The top-level Makefile discovers target `.make` files
automatically.
