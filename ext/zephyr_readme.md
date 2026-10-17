# Zephyr tree for the ChibiOS Zephyr bindings

The Zephyr tree used by the Zephyr sandbox guest, `os/various/zephyr_bindings`,
is not distributed with ChibiOS. This file describes how to fetch it as
`ext/zephyr` and how to prepare it. The `demos/various/SB-CLIENT-ZEPHYR` demo
uses `ext/zephyr` when `ZEPHYR_BASE` is not set.

## Requirements

- Git.
- Python 3.12 or later with the venv module, `python3-venv` on Debian and
  Ubuntu.
- An ARM GCC toolchain in `PATH`, for example the ChibiStudio one.
- Optional, the devicetree compiler `dtc`, `device-tree-compiler` on Debian and
  Ubuntu, Zephyr uses it for additional checks.

The commands below are for a POSIX shell and must be run from the ChibiOS root
directory. They have not been tested on Windows.

## Setup

1. Fetch the Zephyr revision the bindings are tested with. Samples, tests and
   documentation are not checked out, the tree takes about 400 MB:

   ```sh
   git init ext/zephyr
   git -C ext/zephyr sparse-checkout set --no-cone '/*' '!/samples/' '!/tests/' '!/doc/' '!doc/'
   git -C ext/zephyr fetch --depth 1 https://github.com/zephyrproject-rtos/zephyr a915b2f12675b48b16d56653ea145dcd8f20cbbf
   git -C ext/zephyr checkout FETCH_HEAD
   ```

2. Apply the patch adding the external architecture support, see
   `os/various/zephyr_bindings/readme.txt`:

   ```sh
   git -C ext/zephyr apply ../../os/various/zephyr_bindings/patches/zephyr-arch-external.patch
   ```

3. Create a Python environment with the Zephyr requirements, CMake and Ninja:

   ```sh
   python3 -m venv ext/zephyr-venv
   ext/zephyr-venv/bin/pip install -r ext/zephyr/scripts/requirements-base.txt cmake ninja
   ```

## Building

```sh
. ext/zephyr-venv/bin/activate
cmake -GNinja -S demos/various/SB-CLIENT-ZEPHYR -B demos/various/SB-CLIENT-ZEPHYR/build
ninja -C demos/various/SB-CLIENT-ZEPHYR/build
```

The sandbox image is `demos/various/SB-CLIENT-ZEPHYR/build/zephyr/zephyr.elf`.
The sandbox slot is selected with `-DSB_SLOT=<slot>`, see the overlays in
`demos/various/SB-CLIENT-ZEPHYR/slots`.

The ARM GCC found in `PATH` is used, `ZEPHYR_TOOLCHAIN_VARIANT` selects a
different Zephyr toolchain. West is not required.

## Updating

When the revision above or the patch changes, discard the previous patch,
fetch the new revision and apply the patch again:

```sh
git -C ext/zephyr reset --hard
git -C ext/zephyr fetch --depth 1 https://github.com/zephyrproject-rtos/zephyr <revision>
git -C ext/zephyr checkout FETCH_HEAD
git -C ext/zephyr apply ../../os/various/zephyr_bindings/patches/zephyr-arch-external.patch
```

## Using another Zephyr tree

An existing Zephyr tree, also inside a west workspace, can be used by setting
`ZEPHYR_BASE` to it, the same patch must be applied to it.
