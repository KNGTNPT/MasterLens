#!/bin/bash
# 已登录 gh 之后，一条命令完成：绑定 SSH key → 建仓库 → 推送 → 发 Release 上传 DMG
#
# 用法：
#   ./scripts/publish_with_gh.sh [仓库名] [--public|--private]
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
GH="$ROOT/.tools/gh-bin/gh"
REPO_NAME="${1:-MasterLens}"
VISIBILITY="${2:---public}"
VERSION="2.0.3"
TAG="${TAG:-v2.0.3}"
DMG="$ROOT/dist/MasterLens-$VERSION-macOS.dmg"

if [ ! -x "$GH" ]; then
    echo "缺少 gh：$GH"
    echo "请先执行：cd $ROOT && .tools/gh-bin/gh auth login --hostname github.com --git-protocol ssh --web"
    exit 1
fi

if ! "$GH" auth status > /dev/null 2>&1; then
    echo "尚未登录 GitHub。请执行："
    echo "  cd $ROOT"
    echo "  .tools/gh-bin/gh auth login --hostname github.com --git-protocol ssh --web"
    exit 1
fi

"$GH" config set git_protocol ssh > /dev/null 2>&1 || true

echo "== 1/4 绑定 SSH 公钥 =="
if [ -f "$HOME/.ssh/id_ed25519.pub" ]; then
    if "$GH" ssh-key list 2>/dev/null | grep -q "$(cut -d' ' -f2 "$HOME/.ssh/id_ed25519.pub")"; then
        echo "   公钥已在账户中"
    else
        "$GH" ssh-key add "$HOME/.ssh/id_ed25519.pub" \
              --title "MasterLens ($(hostname))" && echo "   已上传公钥" \
              || echo "   上传失败（token 缺少 admin:public_key 权限），请手动添加到 GitHub → Settings → SSH keys"
    fi
fi

cd "$ROOT"

echo "== 2/4 创建 / 复用仓库 =="
if "$GH" repo view "$REPO_NAME" > /dev/null 2>&1; then
    echo "   仓库已存在：$REPO_NAME"
    git remote remove origin 2>/dev/null || true
    git remote add origin "$("$GH" repo view "$REPO_NAME" --json sshUrl --jq .sshUrl)"
    git push -u origin main
else
    "$GH" repo create "$REPO_NAME" "$VISIBILITY" --source=. --remote=origin --push \
          --description "MasterLens - bilingual mastering loudness & mix quality inspector (VST3/AU/Standalone, macOS)"
fi

echo "== 3/4 创建 Release =="
if [ -f "$DMG" ]; then
    if "$GH" release view "$TAG" > /dev/null 2>&1; then
        "$GH" release upload "$TAG" "$DMG" --clobber
    else
        "$GH" release create "$TAG" "$DMG" \
              --title "MasterLens $VERSION" \
              --notes "Universal (arm64 + x86_64) VST3 / AU / Standalone for macOS 11+.
Open the DMG and run 安装 Install.command, then restart your DAW and insert MasterLens on the master bus."
    fi
else
    echo "   未找到 $DMG，请先运行 ./scripts/make_dmg.sh"
fi

echo "== 4/4 完成 =="
"$GH" repo view "$REPO_NAME" --json url --jq .url
