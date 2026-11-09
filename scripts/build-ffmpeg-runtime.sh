#!/bin/bash
# Builds the pinned, LGPL-only FFmpeg runtime that ships with MasterLens.
#
#   scripts/build-ffmpeg-runtime.sh <macos-universal|windows-x64|windows-arm64> [work-dir] [dest-dir]
#
# Sources are pinned by URL and SHA-256; the LAME patch and the exact FFmpeg
# configure line are recorded in the staged build-info.txt. macOS builds both
# slices and lipos them; Windows builds use llvm-mingw so x64 and ARM64 come
# from the same toolchain. The result is staged in
# third_party/ffmpeg/<platform>/ with the licence texts next to it.
set -euo pipefail

PLATFORM="${1:?usage: build-ffmpeg-runtime.sh <macos-universal|windows-x64|windows-arm64> [work-dir] [dest-dir]}"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
WORK="${2:-$ROOT/.tools/ffmpeg-build}"
DEST="${3:-$ROOT/third_party/ffmpeg/$PLATFORM}"

FFMPEG_VERSION="${FFMPEG_VERSION:-9.0.2}"
FFMPEG_URL="${FFMPEG_URL:-https://codeload.github.com/FFmpeg/FFmpeg/tar.gz/refs/tags/n9.0.2}"
FFMPEG_SHA256="${FFMPEG_SHA256:-6e374ed621e48faa40639307dff48ba6fe574a509977956d2cce9669b7cc27e9}"

LAME_VERSION="${LAME_VERSION:-3.100}"
LAME_URL="${LAME_URL:-https://downloads.sourceforge.net/project/lame/lame/3.100/lame-3.100.tar.gz}"
LAME_SHA256="${LAME_SHA256:-ddfe36cab873794038ae2c1210557ad34857a4b6bdc515785d1da9e175b1da1e}"

LLVM_MINGW_VERSION="${LLVM_MINGW_VERSION:-20261104}"
LLVM_MINGW_URL="${LLVM_MINGW_URL:-https://github.com/mstorsjo/llvm-mingw/releases/download/20261104/llvm-mingw-20261104-ucrt-macos-universal.tar.xz}"
LLVM_MINGW_SHA256="${LLVM_MINGW_SHA256:-66cbd5178c4a71b0d473e68ede172f8eb7d15d023e67f39201d0a69e21f6bf3c}"

PATCH="$ROOT/packaging/patches/lame-3.100-c-only-xrpow-init.patch"
JOBS="${JOBS:-$(sysctl -n hw.ncpu 2>/dev/null || getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)}"
mkdir -p "$WORK"
WORK="$(cd "$WORK" && pwd)"

log() { echo "[ffmpeg-runtime] $*"; }

fetch() {
    local url="$1" sha="$2" out="$3"
    if [ ! -f "$out" ]; then
        log "fetching $(basename "$out")"
        curl -fsSL "$url" -o "$out"
    fi
    echo "$sha  $out" | shasum -a 256 -c - > /dev/null
}

extract_once() {
    local archive="$1" dir="$2"
    [ -d "$dir" ] || tar -xf "$archive" -C "$(dirname "$dir")"
    [ -d "$dir" ]
}

prepare_sources() {
    fetch "$FFMPEG_URL" "$FFMPEG_SHA256" "$WORK/ffmpeg-$FFMPEG_VERSION.tar.gz"
    fetch "$LAME_URL" "$LAME_SHA256" "$WORK/lame-$LAME_VERSION.tar.gz"
    extract_once "$WORK/ffmpeg-$FFMPEG_VERSION.tar.gz" "$WORK/FFmpeg-n$FFMPEG_VERSION"
    extract_once "$WORK/lame-$LAME_VERSION.tar.gz" "$WORK/lame-$LAME_VERSION"

    if grep -q 'gfc->init_xrpow_core = init_xrpow_core_sse;' "$WORK/lame-$LAME_VERSION/libmp3lame/quantize.c"; then
        patch -d "$WORK/lame-$LAME_VERSION" -p1 --no-backup-if-mismatch < "$PATCH"
    fi
}

FF_COMPONENTS=(
    --disable-everything
    --enable-ffmpeg
    --enable-libmp3lame
    --enable-protocol=file,pipe
    --enable-demuxer=wav,aiff,flac,mp3,mov,ogg,adts,opus
    --enable-decoder=pcm_u8,pcm_s16le,pcm_s24le,pcm_s32le,pcm_f32le,pcm_f64le,pcm_s16be,pcm_s24be,pcm_s32be,pcm_f32be,flac,mp3,aac,alac,vorbis,opus
    --enable-encoder=aac,libmp3lame,pcm_f32le
    --enable-muxer=mp3,mp4,ipod,wav
    --enable-parser=aac,flac,mpegaudio,opus,vorbis
    --enable-bsf=aac_adtstoasc,extract_extradata,null
    --enable-filter=aresample,aformat,anull
)

FF_COMMON=(
    --disable-autodetect
    --disable-network
    --disable-doc
    --disable-ffplay
    --disable-ffprobe
    --disable-avdevice
    --disable-swscale
    --enable-static
    --disable-shared
)

build_lame_mac() {
    local arch="$1"
    local prefix="$2"
    local bld="$WORK/lame-$LAME_VERSION/bld-mac-$arch"
    [ -x "$prefix/lib/libmp3lame.a" ] && return 0
    rm -rf "$bld" && mkdir -p "$bld"
    log "building LAME $LAME_VERSION ($arch)"
    ( cd "$bld" \
      && ac_cv_header_xmmintrin_h=no ../configure --prefix="$prefix" \
            --disable-shared --enable-static --disable-frontend \
            --disable-dependency-tracking \
            CFLAGS="-arch $arch -O2" LDFLAGS="-arch $arch" > configure.log 2>&1 \
      && make -j"$JOBS" > make.log 2>&1 \
      && make install >> make.log 2>&1 )
}

build_lame_mingw() {
    local triple="$1"
    local prefix="$2"
    local bld="$WORK/lame-$LAME_VERSION/bld-$triple"
    [ -x "$prefix/lib/libmp3lame.a" ] && return 0
    rm -rf "$bld" && mkdir -p "$bld"
    log "building LAME $LAME_VERSION ($triple)"
    ( cd "$bld" \
      && ac_cv_header_xmmintrin_h=no ../configure --prefix="$prefix" --host="$triple" \
            --disable-shared --enable-static --disable-frontend \
            --disable-dependency-tracking \
            CC="$triple-clang" AR="$triple-ar" RANLIB="$triple-ranlib" \
            CFLAGS="-O2" LDFLAGS="-static" > configure.log 2>&1 \
      && make -j"$JOBS" > make.log 2>&1 \
      && make install >> make.log 2>&1 )
}

configure_ffmpeg() {
    local bld="$1" lame="$2"; shift 2
    rm -rf "$bld" && mkdir -p "$bld"
    ( cd "$bld" && GIT_CEILING_DIRECTORIES="$ROOT" \
      ../FFmpeg-n$FFMPEG_VERSION/configure --prefix="$bld/out" \
        "${FF_COMMON[@]}" "${FF_COMPONENTS[@]}" \
        --extra-cflags="-I$lame/include $EXTRA_CFLAGS" \
        --extra-ldflags="-L$lame/lib $EXTRA_LDFLAGS" "$@" > configure.log 2>&1 \
      && make -j"$JOBS" > make.log 2>&1 )
}


prepare_sources

case "$PLATFORM" in
    macos-universal)
        for arch in arm64 x86_64; do
            build_lame_mac "$arch" "$WORK/out/lame-mac-$arch"
        done
        for arch in arm64 x86_64; do
            EXTRA_CFLAGS="-arch $arch"
            EXTRA_LDFLAGS="-arch $arch"
            configure_ffmpeg "$WORK/bld-mac-$arch" "$WORK/out/lame-mac-$arch" \
                --arch="$arch" --target-os=darwin --cc=clang
        done
        mkdir -p "$DEST"
        lipo -create -output "$DEST/ffmpeg" \
            "$WORK/bld-mac-arm64/ffmpeg" "$WORK/bld-mac-x86_64/ffmpeg"
        chmod +x "$DEST/ffmpeg"
        "$ROOT/scripts/verify-runtime.sh" "$DEST/ffmpeg" macos-universal
        FFMPEG_BIN="$DEST/ffmpeg"
        ;;
    windows-x64|windows-arm64)
        if [ "$PLATFORM" = "windows-x64" ]; then
            ARCH=x86_64; TRIPLE=x86_64-w64-mingw32
        else
            ARCH=aarch64; TRIPLE=aarch64-w64-mingw32
        fi
        TOOLCHAIN="$WORK/llvm-mingw-$LLVM_MINGW_VERSION-ucrt-macos-universal"
        if [ ! -d "$TOOLCHAIN" ]; then
            fetch "$LLVM_MINGW_URL" "$LLVM_MINGW_SHA256" "$WORK/llvm-mingw.tar.xz"
            tar -xf "$WORK/llvm-mingw.tar.xz" -C "$WORK"
        fi
        export PATH="$TOOLCHAIN/bin:$PATH"
        build_lame_mingw "$TRIPLE" "$WORK/out/lame-$TRIPLE"
        EXTRA_CFLAGS=""
        EXTRA_LDFLAGS="-static"
        configure_ffmpeg "$WORK/bld-$TRIPLE" "$WORK/out/lame-$TRIPLE" \
            --enable-cross-compile --target-os=mingw32 --arch="$ARCH" \
            --cc="$TRIPLE-clang" --cxx="$TRIPLE-clang++" --ld="$TRIPLE-clang" \
            --ar="$TRIPLE-ar" --ranlib="$TRIPLE-ranlib" --strip="$TRIPLE-strip" \
            --nm="$TRIPLE-nm"
        mkdir -p "$DEST"
        cp "$WORK/bld-$TRIPLE/ffmpeg.exe" "$DEST/ffmpeg.exe"
        file "$DEST/ffmpeg.exe"
        FFMPEG_BIN="$DEST/ffmpeg.exe"
        ;;
    *)
        echo "unknown platform: $PLATFORM" >&2
        exit 2
        ;;
esac

cp "$WORK/FFmpeg-n$FFMPEG_VERSION/COPYING.LGPLv2.1" "$DEST/FFmpeg-COPYING.LGPLv2.1"
cp "$WORK/FFmpeg-n$FFMPEG_VERSION/COPYING.LGPLv3" "$DEST/FFmpeg-COPYING.LGPLv3"
cp "$WORK/lame-$LAME_VERSION/COPYING" "$DEST/LAME-COPYING"

if [ "$PLATFORM" = "macos-universal" ]; then
    CONFIG_LINE="$("$FFMPEG_BIN" -hide_banner -version | sed -n 's/^configuration: //p')"
else
    CONFIG_LINE="$(sed -n 's/^FFMPEG_CONFIGURATION=//p' "$WORK/bld-$TRIPLE/ffbuild/config.mak")"
fi

{
    echo "MasterLens bundled FFmpeg runtime"
    echo "platform: $PLATFORM"
    echo "ffmpeg: $FFMPEG_VERSION  ($FFMPEG_URL)"
    echo "ffmpeg source sha256: $FFMPEG_SHA256"
    echo "lame: $LAME_VERSION  ($LAME_URL)"
    echo "lame source sha256: $LAME_SHA256"
    echo "lame patch: packaging/patches/lame-3.100-c-only-xrpow-init.patch"
    echo "licence: LGPL-2.1-or-later (no --enable-gpl / --enable-nonfree)"
    echo
    echo "configure:"
    echo "$CONFIG_LINE"
} > "$DEST/build-info.txt"


echo "staged $DEST"

