#!/usr/bin/env python3
"""Host-side checker for the RP2350 USB-VALIDATION CDC echo firmware.

Usage:
    python3 host_check.py /dev/ttyACM0

Requires python3 and pyserial only.

Legs:
  1. Echo correctness: deterministic patterns of several sizes, written
     and read back, byte-compared, 50 rounds.
  2. Reset stress: repeated close/open cycles with a 64-byte echo each.

Prints PASS/FAIL per leg and a final "HOST RESULT: PASS|FAIL" line.
Exit status is 0 on PASS, 1 on FAIL, 2 on usage/setup errors.
"""

import sys
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


def echo_once(port, size, seed):
    tx = make_pattern(size, seed)
    port.write(tx)
    port.flush()
    rx = read_exactly(port, size)
    if rx != tx:
        return False, "size=%d rx=%d bytes%s" % (
            size, len(rx),
            "" if len(rx) != len(tx) else " (content mismatch)")
    return True, ""


def leg_echo(path):
    print("[leg 1] echo correctness: sizes=%s rounds=%d" % (SIZES, ROUNDS))
    failures = 0
    with open_port(path) as port:
        for rnd in range(ROUNDS):
            for size in SIZES:
                ok, detail = echo_once(port, size, rnd + size)
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


def leg_reset(path):
    print("[leg 2] reset stress: %d close/open cycles" % RESET_CYCLES)
    failures = 0
    for cyc in range(RESET_CYCLES):
        try:
            with open_port(path) as port:
                ok, detail = echo_once(port, 64, cyc)
                if not ok:
                    failures += 1
                    print("  cycle %d: FAIL %s" % (cyc, detail))
        except (serial.SerialException, OSError) as exc:
            failures += 1
            print("  cycle %d: FAIL open error: %s" % (cyc, exc))
            time.sleep(1.0)
    ok = failures == 0
    print("[leg 2] %s (%d failures)" % ("PASS" if ok else "FAIL", failures))
    return ok


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

    results.append(("reset", leg_reset(path)))

    overall = all(ok for _, ok in results)
    for name, ok in results:
        print("leg %-6s %s" % (name, "PASS" if ok else "FAIL"))
    print("HOST RESULT: %s" % ("PASS" if overall else "FAIL"))
    return 0 if overall else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
