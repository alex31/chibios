# RP2350 USB-VALIDATION

CDC-ACM echo test for the RP USBv1 low level driver (shared by RP2040 and
RP2350). The firmware enumerates as a virtual COM port and echoes back every
byte it receives; a host script drives echo and reset stress legs against it.

The transfer sizes are chosen to exercise the interesting LLD paths:
single-packet transfers, the 63/64/65 packet boundary, exact multiples of
the 64-byte bulk packet size (128, 192, 512), non-multiples (129) and large
multi-chunk transfers (4096, 8192).

## Firmware

Build (requires arm-none-eabi-gcc):

    make

The build produces `build/ch.elf` and `build/ch.bin`. Flash either with
openocd:

    openocd -f interface/cmsis-dap.cfg -f target/rp2350.cfg \
            -c "program build/ch.elf verify reset exit"

or with picotool (device in BOOTSEL mode):

    picotool load -u -x build/ch.elf

- USB: CDC-ACM on the Pico 2 micro-USB connector (EP1 bulk IN/OUT, EP2
  interrupt IN). Shows up as `/dev/ttyACMx` on Linux.
- Heartbeat/statistics: `usb: bytes=N resets=N state=N` on SIOD0
  (GPIO0 = TX, GPIO1 = RX, 38400-8-N-1) every 2 seconds.
- LED (GPIO25) toggles on echo activity.

## Host check

Requires python3 and pyserial, nothing else.

1. Flash the firmware.
2. Plug the Pico 2 USB into the host, wait for the CDC ACM port to appear
   (e.g. `/dev/ttyACM0`).
3. Run:

       python3 host_check.py /dev/ttyACM0

Legs:

1. Echo correctness: for sizes [1, 63, 64, 65, 128, 129, 192, 512, 4096,
   8192] a deterministic pattern is written, read back and byte-compared,
   50 rounds. Every transfer ends with a newline terminator in a short
   final packet so delivery is deterministic: for sizes that are not a
   multiple of 64 the terminator replaces the last pattern byte and the
   wire total equals the advertised size; for multiples of 64 the
   terminator is appended (size + 1 bytes on the wire), since such a
   payload cannot both match the size and end in a short packet. The
   script logs the actual wire total per size in round 0.
2. Reset stress: 25 cycles of a real USB device reset (ioctl
   `USBDEVFS_RESET` on the port's `/dev/bus/usb/BBB/DDD` node) followed
   by a 64-byte echo once the tty reappears. Without permission on the
   usbfs node the leg falls back to plain close/open cycles and reports
   itself as degraded.

The script prints PASS/FAIL per leg and a final `HOST RESULT: PASS|FAIL`
line; the exit status is 0 only on overall PASS.
