#
# Host (desktop/SDL) build environment for application-server and LEA-LVGL.
# Ubuntu 24.04 to match the toolchain/library versions the host build is developed against.
#
FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential gdb cmake ninja-build pkg-config git ca-certificates \
    python3 python3-venv \
    libsdl2-dev \
    libboost-system-dev libboost-filesystem-dev \
    liblmdb-dev libssl-dev libcurl4-openssl-dev libglib2.0-dev \
    libgl1-mesa-dri libegl1 libgles2 \
    curl file imagemagick fonts-dejavu-core \
    && rm -rf /var/lib/apt/lists/*

# Create a user matching the host user so files written to the mounted home directory keep the right ownership.
# The makefile and LEA-LVGL/build.sh rely on $HOME (~/LEA/..., ~/.local), so the home path must match the host's.
ARG USER
ARG HOME_DIR
ARG host_uid
ARG host_gid
RUN userdel -r ubuntu 2>/dev/null || true; \
    groupadd -o -g "${host_gid}" "${USER}" && \
    useradd -o -u "${host_uid}" -g "${host_gid}" -d "${HOME_DIR}" -M -s /bin/bash "${USER}"

USER ${USER}
ENV HOME=${HOME_DIR}
