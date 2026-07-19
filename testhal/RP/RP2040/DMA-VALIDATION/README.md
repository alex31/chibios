# RP2040 DMA-VALIDATION

Validates the RP2040-E13 DMA abort workaround on a Raspberry Pi Pico.

## Purpose

- A timer-paced memory-to-memory transfer aborted mid-flight leaves the
  channel idle (`BUSY` clear).
- No spurious completion interrupt fires for the aborted transfer.
- The partial destination contents are consistent (an exact prefix of
  the source, remainder untouched).
- The channel is immediately reusable for a full-speed transfer that
  completes exactly once with correct data.
- The abort is repeated across many phases of the transfer.

## Build

From repository root:

```sh
cd testhal/RP/RP2040/DMA-VALIDATION
make clean
make -j$(nproc)
```

## Runtime Notes

- Results are printed as `PASS`/`FAIL` lines on UART0 (GPIO0 TX, GPIO1
  RX) at 115200 8N1, followed by a summary.
- The board LED blinks slowly when every check passed and rapidly
  otherwise.
