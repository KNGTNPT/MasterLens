#!/bin/bash
# Verifies a candidate FFmpeg runtime before it is bundled or used in CI.
set -euo pipefail

BIN="${1:?usage: verify-runtime.sh <ffmpeg> [platform]}"
PLATFORM="${2:-unknown}"

[ -f "$BIN" ] || { echo "missing: $BIN" >&2; exit 1; }
[ -x "$BIN" ] || { echo "not executable: $BIN" >&2; exit 1; }

VERSION_OUT="$("$BIN" -hide_banner -version)"
echo "$VERSION_OUT" | head -1

CONFIG="$(echo "$VERSION_OUT" | sed -n 's/^configuration: //p')"
case "$CONFIG" in
    *--enable-gpl*|*--enable-nonfree*)
        echo "refusing GPL / nonfree build: $CONFIG" >&2
        exit 1
        ;;
esac

ENCODERS="$("$BIN" -hide_banner -encoders)"
if ! echo "$ENCODERS" | grep -q ' aac '; then
    echo "AAC encoder missing" >&2
    exit 1
fi

if [ "$PLATFORM" = "macos-universal" ]; then
    ARCH="$(lipo -info "$BIN" 2>/dev/null || true)"
    case "$ARCH" in *arm64*) ;; *) echo "missing arm64 slice: $ARCH" >&2; exit 1 ;; esac
    case "$ARCH" in *x86_64*) ;; *) echo "missing x86_64 slice: $ARCH" >&2; exit 1 ;; esac
fi

echo "runtime OK ($PLATFORM)"
