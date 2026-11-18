#!/bin/bash
#
# Builds the docker image used for host (desktop/SDL) builds of application-server and LEA-LVGL.
# The --build-arg options pass the current user so files created in the container are owned by you.
# Use DOCKER_NO_CACHE=1 DOCKER_PULL_BASE=1 ./build.sh to force a clean rebuild with the latest base image.
#
set -euo pipefail

DOCKER_IMAGE_TAG=lea-trevally-host
SCRIPT_DIR=$(dirname "$(readlink -f "$0")")

DOCKER_BUILD_FLAGS=()
if [[ ${DOCKER_NO_CACHE:-0} == 1 ]]; then
    DOCKER_BUILD_FLAGS+=(--no-cache)
fi
if [[ ${DOCKER_PULL_BASE:-0} == 1 ]]; then
    DOCKER_BUILD_FLAGS+=(--pull)
fi

docker build "${DOCKER_BUILD_FLAGS[@]}" --tag "${DOCKER_IMAGE_TAG}" \
    --build-arg "USER=$(whoami)" \
    --build-arg "HOME_DIR=${HOME}" \
    --build-arg "host_uid=$(id -u)" \
    --build-arg "host_gid=$(id -g)" \
    "$SCRIPT_DIR"
