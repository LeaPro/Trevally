#!/bin/bash
#
# Runs a command (default: bash) in the host-build container, from the current directory.
# Your home directory is mounted at the same path, and the desktop display is passed through
# so the SDL window opens on your screen.
#
# e.g. from Trevally/application-server:
#   docker/run.sh ~/LEA/LEA-LVGL/build.sh      # build + install LVGL into ~/.local
#   docker/run.sh make -j"$(nproc)"            # build application-server
#   docker/run.sh ./obj-host/application-server
#
set -euo pipefail

DOCKER_IMAGE_TAG=lea-trevally-host

if [[ $# -gt 0 ]]; then
    DOCKER_RUN_COMMAND=("$@")
else
    DOCKER_RUN_COMMAND=(bash)
fi

DOCKER_ARGS=(--rm -i --network host --ipc host --workdir "${PWD}" --volume "${HOME}:${HOME}")
if [[ -t 0 && -t 1 ]]; then
    DOCKER_ARGS+=(-t)
fi

# X11 (also covers Wayland desktops via XWayland)
if [[ -n ${DISPLAY:-} ]]; then
    DOCKER_ARGS+=(--env "DISPLAY=${DISPLAY}" --volume /tmp/.X11-unix:/tmp/.X11-unix:ro)
    if [[ -n ${XAUTHORITY:-} && -f ${XAUTHORITY} ]]; then
        DOCKER_ARGS+=(--env "XAUTHORITY=${XAUTHORITY}" --volume "${XAUTHORITY}:${XAUTHORITY}:ro")
    fi
fi

# native Wayland (only used if SDL_VIDEODRIVER=wayland is set)
if [[ -n ${WAYLAND_DISPLAY:-} && -n ${XDG_RUNTIME_DIR:-} && -S ${XDG_RUNTIME_DIR}/${WAYLAND_DISPLAY} ]]; then
    DOCKER_ARGS+=(--env "WAYLAND_DISPLAY=${WAYLAND_DISPLAY}" --env "XDG_RUNTIME_DIR=${XDG_RUNTIME_DIR}"
                  --volume "${XDG_RUNTIME_DIR}/${WAYLAND_DISPLAY}:${XDG_RUNTIME_DIR}/${WAYLAND_DISPLAY}")
fi
if [[ -n ${SDL_VIDEODRIVER:-} ]]; then
    DOCKER_ARGS+=(--env "SDL_VIDEODRIVER=${SDL_VIDEODRIVER}")
fi

# GPU acceleration where available (SDL falls back to software rendering otherwise)
if [[ -d /dev/dri ]]; then
    DOCKER_ARGS+=(--device /dev/dri)
fi

exec docker run "${DOCKER_ARGS[@]}" "${DOCKER_IMAGE_TAG}" "${DOCKER_RUN_COMMAND[@]}"
