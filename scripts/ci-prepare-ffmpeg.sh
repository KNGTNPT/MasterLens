#!/bin/bash
# CI helper: stage the bundled FFmpeg runtime, or fall back explicitly.
#
#   ci-prepare-ffmpeg.sh <macos-universal|windows-x64|windows-arm64> <dest-dir>
#
# Exports for later steps:
#   MASTERLENS_FFMPEG          absolute runtime path (tests + verification)
#   MASTERLENS_REQUIRE_RUNTIME=1 when a runtime is present
#   MASTERLENS_ALLOW_SYSTEM_FFMPEG=1 only for prerelease/PR builds without it
#   ALLOW_MISSING_RUNTIME=1    packaging may omit the runtime (prerelease only)
set -euo pipefail

PLATFORM="${1:?usage: ci-prepare-ffmpeg.sh <platform> <dest-dir>}"
DEST="${2:?missing destination directory}"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"

BIN_NAME="ffmpeg"
case "$PLATFORM" in windows-*) BIN_NAME="ffmpeg.exe" ;; esac
BIN="$DEST/$BIN_NAME"

add_env() {
    if [ -n "${GITHUB_ENV:-}" ]; then
        echo "$1" >> "$GITHUB_ENV"
    fi
}

absolute_path() {
    local path="$1"

    if command -v cygpath >/dev/null 2>&1; then
        cygpath -w "$path"
    else
        echo "$path"
    fi
}

if [ -f "$BIN" ]; then
    "$ROOT/scripts/verify-runtime.sh" "$BIN" "$PLATFORM"
    add_env "MASTERLENS_FFMPEG=$(absolute_path "$PWD/$BIN")"
    add_env "MASTERLENS_REQUIRE_RUNTIME=1"
    echo "using bundled runtime: $PWD/$BIN"
    exit 0
fi

KEY="$(echo "$PLATFORM" | tr 'a-z-' 'A-Z_')"
URL_VAR="FFMPEG_${KEY}_URL"
SHA_VAR="FFMPEG_${KEY}_SHA256"

if [ -n "${!URL_VAR:-}" ] && [ -n "${!SHA_VAR:-}" ]; then
    "$ROOT/scripts/fetch-ffmpeg.sh" "$PLATFORM" "$DEST"
    "$ROOT/scripts/verify-runtime.sh" "$BIN" "$PLATFORM"
    add_env "MASTERLENS_FFMPEG=$(absolute_path "$PWD/$BIN")"
    add_env "MASTERLENS_REQUIRE_RUNTIME=1"
    echo "fetched bundled runtime: $PWD/$BIN"
    exit 0
fi

if [ "${GITHUB_REF_TYPE:-}" = "tag" ]; then
    case "${GITHUB_REF_NAME:-}" in
        *-*) : ;;
        *)
            echo "::error::Stable releases require $URL_VAR and $SHA_VAR (LGPL FFmpeg archive)."
            exit 1
            ;;
    esac
fi

echo "::warning::No FFmpeg runtime configured; development build will use system ffmpeg for tests."
add_env "ALLOW_MISSING_RUNTIME=1"
add_env "MASTERLENS_ALLOW_SYSTEM_FFMPEG=1"
exit 0
