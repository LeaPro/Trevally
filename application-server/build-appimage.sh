#!/bin/bash
# Builds the host (x86-64, SDL2) application-server and packages it as an
# AppImage with all shared libraries bundled. Output: AppImage/Trevally-x86_64.AppImage
#
# Run from a shell WITHOUT the Yocto SDK environment sourced.
# Note: AppImages only run on distros with glibc >= the build machine's, so
# build on the oldest distro you want to support.
set -euo pipefail
cd "$(dirname "$0")"

if [ -n "${SDKTARGETSYSROOT:-}" ]; then
  echo "SDKTARGETSYSROOT is set; open a shell without the SDK environment." >&2
  exit 1
fi

OUT=AppImage
TOOLS=$OUT/tools
APPDIR=$OUT/AppDir
LINUXDEPLOY=$TOOLS/linuxdeploy-x86_64.AppImage
APPIMAGETOOL=$TOOLS/appimagetool-x86_64.AppImage

mkdir -p "$TOOLS"
[ -x "$LINUXDEPLOY" ] || { curl -fL -o "$LINUXDEPLOY" https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage; chmod +x "$LINUXDEPLOY"; }
[ -x "$APPIMAGETOOL" ] || { curl -fL -o "$APPIMAGETOOL" https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-x86_64.AppImage; chmod +x "$APPIMAGETOOL"; }

# Host build into its own object dir so the cross-build in obj/ is untouched.
make -j"$(nproc)"

[ -f "$OUT/trevally.desktop" ] || cat > "$OUT/trevally.desktop" <<'EOF'
[Desktop Entry]
Type=Application
Name=Trevally
Exec=application-server
Icon=trevally
Categories=Utility;
Terminal=true
EOF

# linuxdeploy requires a standard icon size; make a placeholder if missing.
[ -f "$OUT/trevally.png" ] || convert -size 256x256 xc:'#1a1a2e' -fill '#ff2e63' \
  -gravity center -pointsize 140 -annotate 0 'T' "$OUT/trevally.png"

rm -rf "$APPDIR"

# LVGL is installed in ~/.local/lib; let linuxdeploy resolve it.
export LD_LIBRARY_PATH="$HOME/.local/lib:${LD_LIBRARY_PATH:-}"
export APPIMAGE_EXTRACT_AND_RUN=1   # works without FUSE
export ARCH=x86_64

"$LINUXDEPLOY" --appdir "$APPDIR" \
  --executable obj-host/application-server \
  --desktop-file "$OUT/trevally.desktop" \
  --icon-file "$OUT/trevally.png"

"$APPIMAGETOOL" "$APPDIR" "$OUT/Trevally-x86_64.AppImage"
echo "Built $OUT/Trevally-x86_64.AppImage"
