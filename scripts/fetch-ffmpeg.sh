#!/bin/bash
# Fetches a pinned, LGPL-only FFmpeg archive and stages its binary + licences.
#
# The archive and checksum are not stored in git. Provide them explicitly:
#   FFMPEG_MACOS_UNIVERSAL_URL / FFMPEG_MACOS_UNIVERSAL_SHA256
#   FFMPEG_WINDOWS_X64_URL     / FFMPEG_WINDOWS_X64_SHA256
#   FFMPEG_WINDOWS_ARM64_URL   / FFMPEG_WINDOWS_ARM64_SHA256
#
# The archive must be an LGPL build (no --enable-gpl / --enable-nonfree) with the
# native `aac` encoder and, for the A/B feature, `libmp3lame`. Record the same
# URL, checksum and build flags in packaging/THIRD-PARTY-NOTICES.md.
set -euo pipefail

PLATFORM="${1:?usage: fetch-ffmpeg.sh <macos-universal|windows-x64|windows-arm64> <dest-dir>}"
DEST="${2:?missing destination directory}"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"

KEY="$(echo "$PLATFORM" | tr 'a-z-' 'A-Z_')"
URL_VAR="FFMPEG_${KEY}_URL"
SHA_VAR="FFMPEG_${KEY}_SHA256"
URL="${!URL_VAR:-}"
SHA="${!SHA_VAR:-}"

if [ -z "$URL" ] || [ -z "$SHA" ]; then
    echo "error: set $URL_VAR and $SHA_VAR to a pinned LGPL FFmpeg archive." >&2
    echo "       See packaging/THIRD-PARTY-NOTICES.md for the expected build." >&2
    exit 2
fi

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

echo "fetching $PLATFORM runtime"
curl -fsSL "$URL" -o "$TMP/pkg"
echo "$SHA  $TMP/pkg" | shasum -a 256 -c - > /dev/null

mkdir -p "$TMP/extract"
if ! tar -xf "$TMP/pkg" -C "$TMP/extract" 2>/dev/null; then
    unzip -q "$TMP/pkg" -d "$TMP/extract"
fi

BIN="$(find "$TMP/extract" -type f \( -name ffmpeg -o -name ffmpeg.exe \) | head -1)"
[ -n "$BIN" ] || { echo "archive contains no ffmpeg binary" >&2; exit 1; }

mkdir -p "$DEST"
NAME="$(basename "$BIN")"
cp "$BIN" "$DEST/$NAME"
chmod +x "$DEST/$NAME"

find "$TMP/extract" -maxdepth 3 -type f \( -iname 'COPYING.LGPL*' -o -iname 'LICENSE*' -o -iname 'README*' \) \
    -exec cp {} "$DEST/" \; 2>/dev/null || true

if [ "$PLATFORM" = "macos-universal" ]; then
    "$ROOT/scripts/verify-runtime.sh" "$DEST/$NAME" "$PLATFORM"
else
    echo "staged $DEST/$NAME; run verify-runtime.sh on Windows before packaging"
fi

echo "runtime staged at $DEST/$NAME"
