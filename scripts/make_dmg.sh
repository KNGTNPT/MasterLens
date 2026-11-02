#!/bin/bash
# 构建 MasterLens（VST3 + AU + Standalone）并打包为 macOS DMG
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
CMAKE_BIN="$ROOT/.tools/cmake-4.4.5-macos-universal/CMake.app/Contents/bin/cmake"
BUILD="${BUILD_DIR:-$ROOT/build-universal}"
ARCHS="${ARCHS:-arm64;x86_64}"
VERSION="$(sed -nE 's/.*kVersion[[:space:]]*=[[:space:]]*"([^"]+)".*/\1/p' "$ROOT/Source/Version.h")"
DIST="$ROOT/dist"
STAGE="$DIST/stage"

[ -x "$CMAKE_BIN" ] || { echo "找不到 cmake：$CMAKE_BIN"; exit 1; }

echo "== 1/4 配置 ($ARCHS) =="
"$CMAKE_BIN" -S "$ROOT" -B "$BUILD" -DCMAKE_BUILD_TYPE=Release \
    -G "Unix Makefiles" -DCMAKE_OSX_ARCHITECTURES="$ARCHS" > /dev/null

echo "== 2/4 编译 =="
"$CMAKE_BIN" --build "$BUILD" --config Release -j 8

echo "== 3/4 准备 DMG 内容 =="
rm -rf "$STAGE"
mkdir -p "$STAGE"
cp -R "$BUILD/MasterLens_artefacts/Release/VST3/MasterLens.vst3" "$STAGE/"
cp -R "$BUILD/MasterLens_artefacts/Release/AU/MasterLens.component" "$STAGE/"
cp -R "$BUILD/MasterLens_artefacts/Release/Standalone/MasterLens.app" "$STAGE/"
if [ -x "$BUILD/MasterLensCli_artefacts/Release/MasterLensCli" ]; then
    cp "$BUILD/MasterLensCli_artefacts/Release/MasterLensCli" "$STAGE/MasterLensCli"
fi
cp "$ROOT/packaging/Install.command" "$STAGE/安装 Install.command"
cp "$ROOT/packaging/Uninstall.command" "$STAGE/卸载 Uninstall.command"
cp "$ROOT/packaging/README-DMG.txt" "$STAGE/使用说明 README.txt"
mkdir -p "$STAGE/Licenses"
cp "$ROOT/Resources/fonts/OFL.txt" "$STAGE/Licenses/Geist-OFL.txt"
chmod +x "$STAGE/安装 Install.command" "$STAGE/卸载 Uninstall.command"

for bundle in "$STAGE/MasterLens.vst3" "$STAGE/MasterLens.component" "$STAGE/MasterLens.app"; do
    codesign --force --deep -s - "$bundle" > /dev/null 2>&1 || true
done

echo "== 4/4 生成 DMG =="
mkdir -p "$DIST"
rm -f "$DIST/MasterLens-$VERSION-macOS.dmg"
hdiutil create -volname "MasterLens $VERSION - BerryStudio" -srcfolder "$STAGE" \
    -fs HFS+ -format UDZO -imagekey zlib-level=9 -ov \
    "$DIST/MasterLens-$VERSION-macOS.dmg" > /dev/null

DESKTOP="$HOME/Desktop"
if [ -d "$DESKTOP" ]; then
    rm -f "$DESKTOP/MasterLens-$VERSION-macOS.dmg"
    cp "$DIST/MasterLens-$VERSION-macOS.dmg" "$DESKTOP/"
fi

echo ""
ls -lh "$DIST/MasterLens-$VERSION-macOS.dmg"
[ -f "$DESKTOP/MasterLens-$VERSION-macOS.dmg" ] && echo "桌面副本：$DESKTOP/MasterLens-$VERSION-macOS.dmg"
echo "完成："$DIST/MasterLens-$VERSION-macOS.dmg
