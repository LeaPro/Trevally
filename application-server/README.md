# application-server

## Building on your local machine (host build)

The Makefile supports two modes:

- **Host build** — compiles natively on dev machine with
  gcc, using LVGL's SDL2 backend so LVGL renders to a desktop window instead
  of a real display. This is the default when the `SDKTARGETSYSROOT`
  environment variable is not set.
- **Target build** — cross-compiles using the Yocto SDK for Trevally
  (DRM/evdev backend). This happens automatically once you have sourced the
  Yocto SDK's environment setup script, which sets `SDKTARGETSYSROOT`;
  `build.sh` does that for you (see "Building for the target" below).

The host build requires LVGL to be built and installed natively first, since
`application-server` links against it.

### Host build in Docker (no system packages needed)

`docker/` contains an Ubuntu 24.04 image with every host-build dependency, so
nothing besides Docker needs to be installed on your machine. Your home
directory is mounted at the same path, and the desktop display is passed
through so the LVGL SDL window opens on your screen.

```bash
cd ~/LEA/Trevally/application-server
docker/build.sh                               # once (and after Dockerfile changes)
docker/run.sh ~/LEA/LEA-LVGL/build.sh         # build + install LVGL into ~/.local
docker/run.sh make -j"$(nproc)"               # build application-server
docker/run.sh ./obj-host/application-server   # run it
docker/run.sh ./build-appimage.sh             # package AppImage/Trevally-x86_64.AppImage
docker/run.sh                                 # or open a shell in the container
```

The AppImage runs directly on the host (no Docker needed). Since the image is
Ubuntu 24.04, it needs glibc 2.39 or newer on the machine that runs it.

LEA-LVGL must be cloned with its `lvgl` submodule
(`git -C ~/LEA/LEA-LVGL submodule update --init`).

### Host build without Docker

Follow the steps below once per machine.

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

## Building for the target (target build)

```bash
cd ~/LEA/Trevally/application-server
./build.sh            # cross-build obj/application-server
./build.sh clean      # make clean, in the same environment
./build.sh -j"$(nproc)"
```

`build.sh` builds the same way the firmware image build does: it runs
`make application-server` from `~/LEA/yocto-scarthgap/build-trevally` inside the
yocto-scarthgap docker container (`docker-run`), whose Makefile sources the
Trevally SDK's environment setup script and runs `make` here. That requires
yocto-scarthgap to be cloned next to this repo, its docker image to be built
(`~/LEA/yocto-scarthgap/docker/build.sh`) and the SDK to be installed
(`make sdk` in build-trevally, which `build-trevally/build.sh` also does).

After a successful build the Makefile copies `obj/application-server` into
`meta-lea-trevally/recipes-core/application-server-service/files`, so the next
image (`build-trevally/build.sh`) ships the binary you just built.
