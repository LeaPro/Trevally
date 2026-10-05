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

Configure it with the host/SDL2 defconfig (shipped in this repo), build, and
install into `~/.local` (no `sudo` required):

```bash
cd ~/LEA/LEA-LVGL
cp ~/LEA/Trevally/application-server/lvgl/trevally_host.defconfig .config
cmake -S . -B build -G Ninja \
  -DBUILD_SHARED_LIBS=ON -DLV_BUILD_INSTALL=ON \
  -DCMAKE_INSTALL_PREFIX=$HOME/.local
cmake --build build -j"$(nproc)"
cmake --install build
```

This installs LVGL's headers to `~/.local/include/lvgl` and
`liblvgl.so`/`liblvgl_linux.so` to `~/.local/lib`. Re-run these steps
whenever you pull LVGL fork updates.

### 3. Build application-server

From this repo, with `SDKTARGETSYSROOT` unset:

```bash
make -j"$(nproc)"
```

The resulting binary is `obj/application-server`. The Makefile adds an
rpath pointing at `~/.local/lib`, so it will find the LVGL shared libraries
at runtime without needing `LD_LIBRARY_PATH` set.
