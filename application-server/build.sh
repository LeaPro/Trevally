#!/bin/bash
# Cross-builds application-server for the target the same way the image build does: in the
# yocto-scarthgap docker container, via build-trevally's Makefile, which also copies the binary
# into meta-lea-trevally for the next image.
#
# usage: ./build.sh [make goals/flags, e.g. clean or -j8]
SCRIPT_DIR=$(dirname "$(readlink -f "$0")")
BUILD_TREVALLY_DIR=$(realpath -m "$SCRIPT_DIR/../../yocto-scarthgap/build-trevally")
if [[ ! -x $BUILD_TREVALLY_DIR/docker-run ]] ; then
  echo "$BUILD_TREVALLY_DIR/docker-run not found: yocto-scarthgap must be cloned next to Trevally (e.g. ~/LEA/yocto-scarthgap)"
  exit 1
fi
cd "$BUILD_TREVALLY_DIR" && exec ./docker-run make application-server "$@"
