# application-server

## Building on your local machine (host build)

The Makefile supports two modes:

- **Host build** — compiles natively on dev machine with
  gcc, using LVGL's SDL2 backend so LVGL renders to a desktop window instead
  of a real display. This is the default when the `SDKTARGETSYSROOT`
  environment variable is not set.
- **Target build** — cross-compiles using the Yocto SDK for Trevally
  (DRM/evdev backend). This happens automatically once you have sourced the
  Yocto SDK's environment setup script, which sets `SDKTARGETSYSROOT`.

The host build requires LVGL to be built and installed natively first, since
`application-server` links against it. Follow the steps below once per
machine.

### 1. Install system dependencies

```bash
sudo apt install build-essential cmake python3 python3-venv ninja-build \
  libsdl2-dev libboost-system-dev libboost-filesystem-dev \
  liblmdb-dev libssl-dev libcurl4-openssl-dev pkg-config
```

### 2. Clone LVGL and build it natively

Clone LEA's LVGL fork (with submodules) somewhere alongside this repo:

```bash
git clone --recurse-submodules https://github.com/LeaPro/LEA-LVGL.git ~/LEA/LEA-LVGL
```

Build and install it into `~/.local` (no `sudo` required) using its own
build script, which handles the host/SDL2 defconfig, shared-lib build, and
install in one step:

```bash
~/LEA/LEA-LVGL/build.sh
```

This installs LVGL's headers to `~/.local/include/lvgl` and
`liblvgl.so`/`liblvgl_linux.so` to `~/.local/lib`. Re-run this script
whenever you pull LVGL fork updates.

### 3. Build application-server

From this repo, with `SDKTARGETSYSROOT` unset:

```bash
make -j"$(nproc)"
```

The resulting binary is `obj-host/application-server` (target builds use `obj/`). The Makefile adds an
rpath pointing at `~/.local/lib`, so it will find the LVGL shared libraries
at runtime without needing `LD_LIBRARY_PATH` set.
