# Third-party notices — MasterLens

## FFmpeg (bundled runtime)

MasterLens ships a trimmed FFmpeg executable for local AAC / MP3 encode-decode
used by the file QC A/B and the CLI `--codec` round-trip.

- Project: FFmpeg, https://ffmpeg.org
- Version: **9.0.2**, built from
  `https://codeload.github.com/FFmpeg/FFmpeg/tar.gz/refs/tags/n9.0.2`
  (SHA-256 `6e374ed621e48faa40639307dff48ba6fe574a509977956d2cce9669b7cc27e9`).
- MP3 encoder: **LAME 3.100**, built from
  `https://downloads.sourceforge.net/project/lame/lame/3.100/lame-3.100.tar.gz`
  (SHA-256 `ddfe36cab873794038ae2c1210557ad34857a4b6bdc515785d1da9e175b1da1e`),
  with `packaging/patches/lame-3.100-c-only-xrpow-init.patch`.
- Licence: **LGPL-2.1-or-later** for the builds we accept. Builds configured with
  `--enable-gpl`, `--enable-nonfree` or GPL-only components (x264, x265, …) are
  rejected by `scripts/verify-runtime.sh` and must not be shipped.
- Required capability: native `aac` encoder (and `libmp3lame` for MP3 A/B).
- Build: `scripts/build-ffmpeg-runtime.sh` (macOS Universal via two clang
  slices + `lipo`; Windows x64 / ARM64 via pinned llvm-mingw). It disables
  everything except file/pipe protocols, WAV / AIFF / FLAC / MP3 / MOV / OGG /
  ADTS demuxing, PCM / FLAC / MP3 / AAC / ALAC / Vorbis / Opus decoding,
  AAC / libmp3lame / `pcm_f32le` encoding, MP3 / MP4 / iPod / WAV muxing and
  the `aresample` / `aformat` / `anull` filters.
- The exact configure line, source URLs and checksums are written next to each
  binary in `third_party/ffmpeg/<platform>/build-info.txt`; the LGPL texts and
  the LAME licence are copied beside it and into the installer's `Licenses/`.
- Committed runtimes (sizes are the staged executables):

  | platform | file | size |
  | --- | --- | --- |
  | macOS Universal | `third_party/ffmpeg/macos-universal/ffmpeg` | 6.6 MB |
  | Windows x64 | `third_party/ffmpeg/windows-x64/ffmpeg.exe` | 3.4 MB |
  | Windows ARM64 | `third_party/ffmpeg/windows-arm64/ffmpeg.exe` | 2.8 MB |

- The runtime is invoked as a separate process. Replacing
  `Contents/Resources/ffmpeg` (macOS) or `ffmpeg\ffmpeg.exe` (Windows) with a
  compatible build is supported; no re-signing of MasterLens is required for
  source builds of FFmpeg.
- FFmpeg and LAME are provided without warranty. See the `FFmpeg-COPYING.LGPLv2.1`
  / `FFmpeg-COPYING.LGPLv3` / `LAME-COPYING` files distributed with the runtime
  for the full licence text; corresponding source is the pinned archives above.


The runtime version reported by MasterLens (About, codec menu, CLI
`Codec backend:` line, JSON `backend` / `ffmpegVersion`) is the version actually
invoked, not a hard-coded string.

## Geist fonts

Geist is distributed under the SIL Open Font License; see
`Resources/fonts/OFL.txt`.

## JUCE

JUCE is used under its own licence; see https://juce.com for details.
