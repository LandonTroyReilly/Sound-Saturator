#!/bin/bash
# Build an Apple Silicon installer for Sound Saturator and install it
# for the current user at ~/Library/Audio/Plug-Ins/VST3.

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
ARCH="$(uname -m)"

if [[ "$(uname -s)" != "Darwin" || "$ARCH" != "arm64" ]]; then
    echo "Sound Saturator’s installer is for Apple Silicon Macs (arm64)." >&2
    exit 1
fi

VERSION="$(sed -n 's/^project(SoundSaturator VERSION \([0-9.][0-9.]*\).*/\1/p' "$ROOT/CMakeLists.txt")"
if [[ -z "$VERSION" ]]; then
    echo "Could not read the project version from CMakeLists.txt." >&2
    exit 1
fi

BUILD_DIR="$ROOT/build"
VST3="$BUILD_DIR/SoundSaturator_artefacts/Release/VST3/Sound Saturator.vst3"
BINARY="$VST3/Contents/MacOS/Sound Saturator"

if [[ "${SOUND_SATURATOR_SKIP_BUILD:-}" != "1" ]]; then
    cmake -S "$ROOT" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release
    cmake --build "$BUILD_DIR" --config Release --target SoundSaturator_VST3 -j "$(sysctl -n hw.ncpu)"
fi

if [[ ! -d "$VST3" || ! -f "$BINARY" ]]; then
    echo "The Release VST3 was not produced at:" >&2
    echo "  $VST3" >&2
    exit 1
fi

if ! file "$BINARY" | grep -q "arm64"; then
    echo "The built plugin is not an arm64 binary." >&2
    exit 1
fi

STAGE="$(mktemp -d)"
cleanup() { rm -rf "$STAGE"; }
trap cleanup EXIT

PAYLOAD="$STAGE/payload"
mkdir -p "$PAYLOAD"
ditto "$VST3" "$PAYLOAD/Sound Saturator.vst3"

COMPONENT="$STAGE/SoundSaturator-component.pkg"
pkgbuild \
    --root "$PAYLOAD" \
    --identifier "com.soundsaturator.prototype.vst3" \
    --version "$VERSION" \
    --install-location "Library/Audio/Plug-Ins/VST3" \
    --ownership recommended \
    "$COMPONENT"

DIST_XML="$STAGE/distribution.xml"
cat > "$DIST_XML" <<EOF
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="2">
    <title>Sound Saturator</title>
    <options customize="never" require-scripts="false" hostArchitectures="arm64"/>
    <domains enable_anywhere="false" enable_currentUserHome="true" enable_localSystem="false"/>
    <volume-check>
        <allowed-os-versions>
            <os-version min="12.0"/>
        </allowed-os-versions>
    </volume-check>
    <choices-outline>
        <line choice="com.soundsaturator.prototype.vst3"/>
    </choices-outline>
    <choice id="com.soundsaturator.prototype.vst3" title="Sound Saturator" description="Apple Silicon VST3 audio effect">
        <pkg-ref id="com.soundsaturator.prototype.vst3"/>
    </choice>
    <pkg-ref id="com.soundsaturator.prototype.vst3" version="$VERSION" auth="none">SoundSaturator-component.pkg</pkg-ref>
</installer-gui-script>
EOF

OUT_DIR="$ROOT/dist"
mkdir -p "$OUT_DIR"
PKG="$OUT_DIR/Sound Saturator-${VERSION}-arm64.pkg"
productbuild --distribution "$DIST_XML" --package-path "$STAGE" "$PKG"

echo "Installer: $PKG"
installer -pkg "$PKG" -target CurrentUserHomeDirectory

INSTALLED="$HOME/Library/Audio/Plug-Ins/VST3/Sound Saturator.vst3/Contents/MacOS/Sound Saturator"
if ! file "$INSTALLED" | grep -q "arm64"; then
    echo "The installed plugin is not arm64." >&2
    exit 1
fi

echo "Installed: $INSTALLED"
