# RP2040 RTC-VALIDATION

This test application validates the RP2040 RTC alarm contract.

## Purpose

- A programmed alarm fires and invokes the registered callback.
- `rtcGetAlarm()` reflects the armed alarm.
- `rtcSetAlarm(..., NULL)` disables the alarm instead of faulting.
- An alarm specification with an all-zero mask disables the alarm.
- Both disable forms update the saved state read by `rtcGetAlarm()`.
- A disabled alarm no longer fires.

## Build

From repository root:

```sh
cd testhal/RP/RP2040/RTC-VALIDATION
make clean
make -j$(nproc)
```

Produced artifacts are in the local `build/` directory (`ch.elf`, `ch.hex`, `ch.bin`).

## Configuration Notes

- HAL RTC and HAL UART are enabled in [cfg/halconf.h](cfg/halconf.h).
- The RTC interrupt priority is set in [cfg/mcuconf.h](cfg/mcuconf.h).

## Runtime Notes

- Results are printed as `PASS`/`FAIL` lines on UART0 (GPIO0 TX, GPIO1 RX)
  at 115200 8N1, followed by a summary.
- The board LED blinks slowly when every check passed and rapidly otherwise.
