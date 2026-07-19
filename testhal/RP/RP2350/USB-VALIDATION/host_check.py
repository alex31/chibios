#!/usr/bin/env python3
"""Host-side checker for the RP2350 USB-VALIDATION CDC echo firmware.

Usage:
    python3 host_check.py /dev/ttyACM0

Requires python3 and pyserial, on a Linux host: the script imports fcntl
and drives USBDEVFS_RESET through the tty's usbfs node, neither of which
exists on Windows or macOS.

Legs:
  1. Echo correctness: deterministic patterns of several sizes, written
     and read back, byte-compared, 50 rounds.
  2. Reset stress: repeated real USB device resets (USBDEVFS_RESET on the
     tty's usbfs node) with a 64-byte echo after each. Without usbfs
     access it degrades to plain close/open cycles.

Prints PASS/FAIL per leg and a final "HOST RESULT: PASS|FAIL" line.
Exit status is 0 on PASS, 1 on FAIL, 2 on usage/setup errors.
"""

import errno
import fcntl
import os
import sys
import threading
import time

try:
    import serial
except ImportError:
    print("pyserial is required: pip install pyserial", file=sys.stderr)
    sys.exit(2)

# Sizes chosen to hit the interesting LLD paths: single packet, packet
# boundary (63/64/65), exact multiples of the 64-byte bulk packet size
# (128, 192, 512), non-multiples (129), and large multi-chunk transfers.
SIZES = [1, 63, 64, 65, 128, 129, 192, 512, 4096, 8192]
ROUNDS = 50
RESET_CYCLES = 25
READ_TIMEOUT = 5.0
TTY_REAPPEAR_TIMEOUT = 10.0

# From linux/usbdevice_fs.h: _IO('U', 20).
USBDEVFS_RESET = 0x5514


def make_pattern(size, seed):
    """Deterministic, seed-dependent byte pattern."""
    return bytes(((seed * 31) + (i * 7) + (i >> 8)) & 0xFF
                 for i in range(size))


def open_port(path):
    port = serial.Serial(path,
                         baudrate=115200,
                         timeout=READ_TIMEOUT,
                         write_timeout=READ_TIMEOUT)
    # Let the device-side CDC settle, then drop any stale bytes.
    time.sleep(0.2)
    port.reset_input_buffer()
    return port


def read_exactly(port, count):
    """Read exactly count bytes or give up at the deadline."""
    data = b""
    deadline = time.monotonic() + READ_TIMEOUT
    while len(data) < count and time.monotonic() < deadline:
        chunk = port.read(count - len(data))
        if chunk:
            data += chunk
    return data


def wire_pattern(size, seed):
    """Bytes actually transmitted for an advertised size.

    Bulk OUT transfers only complete on a short packet or when the
    device-side armed length is reached; a write that is an exact
    multiple of the 64-byte packet size would otherwise sit in an
    unfinished transfer until later traffic, so every transfer ends
    with a newline terminator in a short final packet. For sizes that
    are not a multiple of 64 the terminator replaces the last pattern
    byte and the wire total equals the advertised size. A payload that
    is a multiple of 64 cannot both match the advertised size and end
    in a short packet, so there the terminator is appended and the wire
    total is size + 1; the extra byte is the documented cost.
    """
    if (size % 64) != 0:
        return make_pattern(size - 1, seed) + b"\n"
    return make_pattern(size, seed) + b"\n"


def echo_once(port, size, seed, log_wire=False):
    tx = wire_pattern(size, seed)
    if log_wire:
        print("  size %d -> wire %d" % (size, len(tx)))
    # Large transfers exceed the combined device/host buffering, so the
    # echo must be drained concurrently with the write or both sides
    # deadlock on full queues. A reader thread collects the echo while
    # the write proceeds.
    rx_buf = bytearray()
    done = threading.Event()

    def reader():
        deadline = time.monotonic() + READ_TIMEOUT
        while len(rx_buf) < len(tx) and time.monotonic() < deadline:
            chunk = port.read(len(tx) - len(rx_buf))
            if chunk:
                rx_buf.extend(chunk)
        done.set()

    t = threading.Thread(target=reader)
    t.start()
    try:
        port.write(tx)
        port.flush()
    except serial.SerialTimeoutException:
        done.wait(READ_TIMEOUT)
        t.join()
        return False, "size=%d WRITE TIMEOUT" % size
    t.join()
    rx = bytes(rx_buf)
    if rx != tx:
        return False, "size=%d wire=%d rx=%d bytes%s" % (
            size, len(tx), len(rx),
            "" if len(rx) != len(tx) else " (content mismatch)")
    return True, ""


def leg_echo(path):
    print("[leg 1] echo correctness: sizes=%s rounds=%d" % (SIZES, ROUNDS))
    failures = 0
    with open_port(path) as port:
        for rnd in range(ROUNDS):
            for size in SIZES:
                ok, detail = echo_once(port, size, rnd + size,
                                       log_wire=(rnd == 0))
                if not ok:
                    failures += 1
                    print("  round %d: FAIL %s" % (rnd, detail))
                    # Drain whatever is left before continuing.
                    time.sleep(0.5)
                    port.reset_input_buffer()
            if (rnd + 1) % 10 == 0:
                print("  round %d/%d done" % (rnd + 1, ROUNDS))
    ok = failures == 0
    print("[leg 1] %s (%d failures)" % ("PASS" if ok else "FAIL", failures))
    return ok


def resolve_usbfs_node(tty_path):
    """Map a CDC tty to its usbfs node /dev/bus/usb/BBB/DDD.

    /sys/class/tty/<name>/device points at the USB interface directory;
    the USB device directory above it carries the busnum/devnum files.
    Walk upwards until both are found.
    """
    name = os.path.basename(tty_path)
    node = os.path.realpath("/sys/class/tty/%s/device" % name)
    while node not in ("/", ""):
        busnum = os.path.join(node, "busnum")
        devnum = os.path.join(node, "devnum")
        if os.path.isfile(busnum) and os.path.isfile(devnum):
            with open(busnum) as f:
                bus = int(f.read().strip())
            with open(devnum) as f:
                dev = int(f.read().strip())
            return "/dev/bus/usb/%03d/%03d" % (bus, dev)
        node = os.path.dirname(node)
    raise FileNotFoundError(
        errno.ENOENT, "no busnum/devnum in sysfs for %s" % tty_path)


def usb_reset(tty_path):
    """Issue a real USB device reset (ioctl USBDEVFS_RESET) for the tty."""
    node = resolve_usbfs_node(tty_path)
    fd = os.open(node, os.O_WRONLY)
    try:
        fcntl.ioctl(fd, USBDEVFS_RESET, 0)
    finally:
        os.close(fd)


def wait_for_tty(path, timeout):
    """Wait for the tty to (re)appear and accept an open, or None."""
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if os.path.exists(path):
            try:
                return open_port(path)
            except (serial.SerialException, OSError):
                pass
        time.sleep(0.2)
    return None


def leg_reset(path):
    """Reset stress leg.

    Each cycle issues a real USB device reset via USBDEVFS_RESET on the
    tty's usbfs node (closing/reopening a CDC tty does not reset the USB
    device), waits for the tty to come back and runs a 64-byte echo.
    Without permission to open the usbfs node the leg degrades to the
    old close/open cycles; that is reported and marked in the result.

    Returns (ok, degraded).
    """
    print("[leg 2] reset stress: %d USB reset cycles" % RESET_CYCLES)
    failures = 0
    resets = 0
    degraded = False
    for cyc in range(RESET_CYCLES):
        if not degraded:
            try:
                usb_reset(path)
                resets += 1
            except OSError as exc:
                if exc.errno in (errno.EACCES, errno.ENOENT):
                    degraded = True
                    print("  RESET LEG DEGRADED: no permission for "
                          "USBDEVFS_RESET, close/open only (%s)" % exc)
                else:
                    failures += 1
                    print("  cycle %d: FAIL reset error: %s" % (cyc, exc))
                    time.sleep(1.0)
                    continue
        port = wait_for_tty(path, TTY_REAPPEAR_TIMEOUT)
        if port is None:
            failures += 1
            print("  cycle %d: FAIL tty did not reappear within %.0fs"
                  % (cyc, TTY_REAPPEAR_TIMEOUT))
            continue
        try:
            with port:
                ok, detail = echo_once(port, 64, cyc, log_wire=(cyc == 0))
                if not ok:
                    failures += 1
                    print("  cycle %d: FAIL %s" % (cyc, detail))
        except (serial.SerialException, OSError) as exc:
            failures += 1
            print("  cycle %d: FAIL echo error: %s" % (cyc, exc))
            time.sleep(1.0)
    print("  real USB resets performed: %d/%d" % (resets, RESET_CYCLES))
    ok = failures == 0
    print("[leg 2] %s (%d failures)%s"
          % ("PASS" if ok else "FAIL", failures,
             " (DEGRADED: close/open only)" if degraded else ""))
    return ok, degraded


def main(argv):
    if len(argv) != 2:
        print("usage: %s <cdc-tty-path>" % argv[0], file=sys.stderr)
        return 2

    path = argv[1]
    results = []

    try:
        results.append(("echo", leg_echo(path)))
    except (serial.SerialException, OSError) as exc:
        print("[leg 1] FAIL: %s" % exc)
        results.append(("echo", False))

    reset_ok, reset_degraded = leg_reset(path)
    results.append(("reset (degraded)" if reset_degraded else "reset",
                    reset_ok))

    overall = all(ok for _, ok in results)
    for name, ok in results:
        print("leg %-16s %s" % (name, "PASS" if ok else "FAIL"))
    print("HOST RESULT: %s" % ("PASS" if overall else "FAIL"))
    return 0 if overall else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
