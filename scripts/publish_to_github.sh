#!/bin/bash
# 把本地仓库发布到 GitHub：创建仓库 → 推送代码 → 建 Release 并上传 DMG
#
# 用法：
#   GH_TOKEN=ghp_xxxxxxxx ./scripts/publish_to_github.sh [仓库名] [public|private]
#
# 需要的 token 权限：repo（classic）或 Contents + Administration（fine-grained）
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
REPO_NAME="${1:-MasterLens}"
VISIBILITY="${2:-public}"
TOKEN="${GH_TOKEN:-${GITHUB_TOKEN:-}}"
VERSION="2.0.2"
TAG="${TAG:-v2.0.2}"
DMG="$ROOT/dist/MasterLens-$VERSION-macOS.dmg"

if [ -z "$TOKEN" ]; then
    echo "缺少 GH_TOKEN。示例："
    echo "  GH_TOKEN=ghp_xxxxxxxx ./scripts/publish_to_github.sh MasterLens public"
    exit 1
fi

api() {
    curl -sS -H "Authorization: Bearer $TOKEN" \
            -H "Accept: application/vnd.github+json" \
            -H "X-GitHub-Api-Version: 2022-11-28" "$@"
}

json_field() {
    python3 -c "import sys,json;print(json.load(sys.stdin).get('$1',''))"
}

echo "== 1/4 校验 token =="
LOGIN="$(api https://api.github.com/user | json_field login)"

if [ -z "$LOGIN" ]; then
    echo "token 无效或网络不可达。"
    exit 1
fi

echo "   登录账号：$LOGIN"

echo "== 2/4 创建仓库 $LOGIN/$REPO_NAME（$VISIBILITY）=="
CREATE_BODY="$(python3 -c "import json;print(json.dumps({'name':'$REPO_NAME','private': '$VISIBILITY'=='private','description':'MasterLens - bilingual mastering loudness & mix quality inspector (VST3/AU/Standalone, macOS)','has_issues':True,'has_wiki':False}))")"
CREATE_CODE="$(api -o /tmp/gh_create.json -w '%{http_code}' -X POST \
    https://api.github.com/user/repos -d "$CREATE_BODY")"

case "$CREATE_CODE" in
    201) echo "   已创建" ;;
    422) echo "   仓库已存在，继续使用" ;;
    *)   echo "   创建失败（HTTP $CREATE_CODE）："; cat /tmp/gh_create.json; exit 1 ;;
esac

echo "== 3/4 推送代码 =="
cd "$ROOT"
git remote remove origin 2>/dev/null || true
git remote add origin "https://github.com/$LOGIN/$REPO_NAME.git"
git push "https://x-access-token:$TOKEN@github.com/$LOGIN/$REPO_NAME.git" main:main

echo "== 4/4 发布 Release 并上传 DMG =="
if [ ! -f "$DMG" ]; then
    echo "   未找到 $DMG，先执行 ./scripts/make_dmg.sh"
else
    RELEASE="$(api -X POST "https://api.github.com/repos/$LOGIN/$REPO_NAME/releases" \
        -d "$(python3 -c "import json;print(json.dumps({'tag_name':'$TAG','name':'MasterLens $VERSION','body':'Universal (arm64 + x86_64) VST3 / AU / Standalone. Open the DMG and run the install script.','draft':False,'prerelease':True}))")")"

    UPLOAD_URL="$(printf '%s' "$RELEASE" | python3 -c "import sys,json;print(json.load(sys.stdin).get('upload_url','').split('{')[0])")"

    if [ -z "$UPLOAD_URL" ]; then
        echo "   Release 创建失败（标签可能已存在）："
        printf '%s\n' "$RELEASE" | head -5
    else
        api -o /tmp/gh_upload.json -X POST \
            -H "Content-Type: application/octet-stream" \
            --data-binary "@$DMG" \
            "$UPLOAD_URL?name=MasterLens-$VERSION-macOS.dmg" > /dev/null
        echo "   已上传：$(cat /tmp/gh_upload.json | python3 -c "import sys,json;print(json.load(sys.stdin).get('browser_download_url','(失败)'))")"
    fi
fi

echo ""
echo "完成：https://github.com/$LOGIN/$REPO_NAME"
