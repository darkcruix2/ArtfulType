#!/usr/bin/env bash
# =============================================================================
# setup_osxcross.sh — Build the osxcross PPC cross-compiler for ArtfulType
#
# PREREQUISITES (installed by the agent):
#   git, clang/llvm, cmake, libxml2-dev, libssl-dev, lzma-dev, uuid-dev
#
# SDK REQUIREMENT (you must supply this):
#   A MacOSX10.4u.sdk tarball is needed.  Apple no longer distributes it
#   publicly, but it ships inside Xcode 2.5 (the last PPC version).
#   Options:
#     A) Download Xcode 2.5 from:
#          https://developer.apple.com/download/more/
#        (Free Apple Developer account required)
#        Then extract:  xar -xf xcode25_8m2558_developerdvd.dmg
#                       pkgutil --expand Xcode.pkg /tmp/xcode_pkg
#                       ... (we'll automate this if you have the .dmg)
#
#     B) If you already have the SDK as a tarball, place it at:
#          /mnt/volume1/MyCode/workspace/ArtfulType/app-carbon/MacOSX10.4u.sdk.tar.xz
#        and re-run this script.
#
#     C) Unofficial pre-packaged tarballs are available from:
#          https://github.com/phracker/MacOSX-SDKs/releases
#        Download: MacOSX10.4u.sdk.tar.xz  (or .bz2)
# =============================================================================

set -euo pipefail

OSXCROSS_DIR=/mnt/volume1/MyCode/workspace/osxcross
SDK_TARBALL_SRC=/mnt/volume1/MyCode/workspace/ArtfulType/app-carbon/MacOSX10.4u.sdk.tar.xz

echo "=== 1. Clone osxcross ==="
if [ ! -d "$OSXCROSS_DIR" ]; then
    git clone --depth 1 https://github.com/tpoechtrager/osxcross.git "$OSXCROSS_DIR"
else
    echo "osxcross already cloned at $OSXCROSS_DIR"
fi

echo ""
echo "=== 2. Check for SDK tarball ==="
if [ ! -f "$SDK_TARBALL_SRC" ]; then
    echo ""
    echo "  ⚠️  SDK tarball not found at: $SDK_TARBALL_SRC"
    echo ""
    echo "  Please place the MacOSX10.4u.sdk.tar.{xz,bz2,gz} there and re-run."
    echo "  Quick option: download from phracker's archive:"
    echo "    wget -P /mnt/volume1/MyCode/workspace/ArtfulType/app-carbon/ \\"
    echo "      https://github.com/phracker/MacOSX-SDKs/releases/download/11.3/MacOSX10.4u.sdk.tar.xz"
    echo ""
    echo "  (Alternatively, extract from Xcode 2.5 .dmg — see instructions above.)"
    exit 1
fi

echo "  SDK tarball found: $SDK_TARBALL_SRC"
cp "$SDK_TARBALL_SRC" "$OSXCROSS_DIR/tarballs/"

echo ""
echo "=== 3. Build osxcross (this takes ~10 minutes) ==="
cd "$OSXCROSS_DIR"
# We build with Clang (faster, better PPC support than system GCC)
UNATTENDED=1 ./build.sh

echo ""
echo "=== 4. Verify PPC compiler ==="
OSXCROSS_BIN="$OSXCROSS_DIR/target/bin"
if [ -x "$OSXCROSS_BIN/powerpc-apple-darwin8-gcc" ]; then
    echo "  ✅  PPC cross-compiler ready:"
    "$OSXCROSS_BIN/powerpc-apple-darwin8-gcc" --version
else
    echo "  ❌  powerpc-apple-darwin8-gcc not found in $OSXCROSS_BIN"
    ls "$OSXCROSS_BIN/" | grep powerpc || true
    exit 1
fi

echo ""
echo "=== 5. Build ArtfulType Carbon ==="
cd /mnt/volume1/MyCode/workspace/ArtfulType/app-carbon
make clean
make SDK="$OSXCROSS_DIR/target/SDK/MacOSX10.4u.sdk" \
     TOOLCHAIN_PREFIX="$OSXCROSS_BIN/powerpc-apple-darwin8-"

echo ""
echo "=== 6. Create app bundle ==="
make bundle \
     SDK="$OSXCROSS_DIR/target/SDK/MacOSX10.4u.sdk" \
     TOOLCHAIN_PREFIX="$OSXCROSS_BIN/powerpc-apple-darwin8-"

echo ""
echo "============================================================"
echo "  Done!  App bundle: app-carbon/ArtfulTypePro.app"
echo "  Copy to a PPC Mac running OS X 10.4+ and double-click."
echo "============================================================"
