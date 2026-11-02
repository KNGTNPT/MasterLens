#!/bin/bash
# MasterLens 一键安装脚本（双击运行）
set -u

SRC="$(cd "$(dirname "$0")" && pwd)"
VST3_DIR="$HOME/Library/Audio/Plug-Ins/VST3"
AU_DIR="$HOME/Library/Audio/Plug-Ins/Components"
APP_DIR="$HOME/Applications"

echo "MasterLens · 母带响度 / 平台归一化 / 信号健康"
echo "by BerryStudio · Developer KNGTNPT 兰"
echo "========================================"

mkdir -p "$VST3_DIR" "$AU_DIR" "$APP_DIR"

install_bundle () {
    local name="$1"
    local dest="$2"

    if [ -d "$SRC/$name" ]; then
        echo "→ 安装 $name"
        rm -rf "$dest/$name"
        cp -R "$SRC/$name" "$dest/$name"
        xattr -dr com.apple.quarantine "$dest/$name" 2>/dev/null
        codesign --force --deep -s - "$dest/$name" >/dev/null 2>&1
    fi
}

install_bundle "MasterLens.vst3" "$VST3_DIR"
install_bundle "MasterLens.component" "$AU_DIR"

if [ -d "$SRC/MasterLens.app" ]; then
    echo "→ 安装独立版到 ~/Applications"
    rm -rf "$APP_DIR/MasterLens.app"
    cp -R "$SRC/MasterLens.app" "$APP_DIR/"
    xattr -dr com.apple.quarantine "$APP_DIR/MasterLens.app" 2>/dev/null
fi

echo "========================================"
echo "安装完成："
echo "  VST3   : $VST3_DIR/MasterLens.vst3"
echo "  AU     : $AU_DIR/MasterLens.component"
echo "  独立版 : $APP_DIR/MasterLens.app"
echo ""
echo "请重启 DAW，在「音频效果 / Analyzer / 分析」分类下找到 MasterLens，"
echo "把它插在总线（Master Bus）或母带链的最后一级即可开始分析。"
echo ""
echo "BerryStudio · 2026"
echo ""
read -n 1 -s -r -p "按任意键关闭窗口..."
echo
