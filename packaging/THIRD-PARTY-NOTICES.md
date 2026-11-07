# Third-party notices — MasterLens

## FFmpeg (bundled runtime)

MasterLens ships an FFmpeg executable for local AAC / MP3 encode-decode used by
the file QC A/B and the CLI `--codec` round-trip.

- Project: FFmpeg, https://ffmpeg.org
- Licence: **LGPL-2.1-or-later** for the builds we accept. Builds configured with
  `--enable-gpl`, `--enable-nonfree` or GPL-only components (x264, x265, …) are
  rejected by `scripts/verify-runtime.sh` and must not be shipped.
- Required capability: native `aac` encoder (and `libmp3lame` for MP3 A/B).
- Pinned version, source archive URL, SHA-256 and exact build flags are recorded
  next to the binary in `third_party/ffmpeg/<platform>/` and are copied into the
  installer's `Licenses/` folder.
- The runtime is invoked as a separate process. Replacing
  `Contents/Resources/ffmpeg` (macOS) or `ffmpeg\ffmpeg.exe` (Windows) with a
  compatible build is supported; no re-signing of MasterLens is required for
  source builds of FFmpeg.
- FFmpeg is provided without warranty. See the `COPYING.LGPLv2.1` /
  `COPYING.LGPLv3` files distributed with the runtime for the full licence text
  and the FFmpeg source link for the corresponding source code.

The runtime version reported by MasterLens (About, codec menu, CLI
`Codec backend:` line, JSON `backend` / `ffmpegVersion`) is the version actually
invoked, not a hard-coded string.

## Geist fonts

Geist is distributed under the SIL Open Font License; see
`Resources/fonts/OFL.txt`.

## JUCE

JUCE is used under its own licence; see https://juce.com for details.
