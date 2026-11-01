# MasterLens 2.0.0-beta.1 — file QC, equal-loudness A/B, host timeline

This beta turns the live inspector into a pre-delivery workflow. It is not a
replacement for listening; it is the fast path between "the mix is done" and
"the files are actually ready to send".

## What changed

- **Offline file QC** (`file/FileAnalyzer.*`): reads WAV / AIFF / FLAC on a worker
  thread at the file's native sample rate, runs `AnalysisEngine` plus a Welch
  spectrum, and returns the same `StatsSnapshot` / `SignalDefects` used live.
  Folders are expanded recursively and results are added to the album snapshot
  automatically.
- **Reference sample-rate fix** (`dsp/ReferenceSpectrum.cpp`): reference files are
  resampled to the project rate before the FFT and loudness pass. The offline
  suite covers 44.1 / 48 / 88.2 / 96 kHz, including 96 k → 48 k.
- **Equal-loudness monitor / reference A/B** (`audio/ReferencePlayer.*`): the
  first reference file is decoded into memory and rendered at the host playhead
  with linear interpolation when rates differ. Both paths are opt-in; bypass is
  still bit-transparent.
- **Host time and coverage**: event timestamps are offset by the host playhead at
  reset. `StatsSnapshot` carries `timeOrigin` / `hostPosition` / `hostLength` /
  `coveragePct`; the album gate blocks a track below 95 % coverage. Hosts do not
  expose a universal track length, so the album page has a manual "total length"
  field.
- **Persistence**: the active album is embedded in APVTS state; `album.json`
  remains the cross-instance cache and a named `Album_<name>.json` sidecar is
  written alongside it. Track records include source, duration, sample rate,
  bit depth, coverage and plug-in version.
- **Explicit out-of-range audition**: a timestamp outside the 12 s history ring
  is rejected with a status message instead of being replaced by "last 1.5 s".
- **Per-channel / Side true peak**: `Stats` now publishes L, R and Side true peak
  (in addition to the existing total), and the stereo page shows them.
- **100% density pass**: base type is 1–2 px larger, rows / cards / gaps are
  tighter, the stereo page no longer centres a fixed-height block, and the
  signal cards fill the available height.

## Verification

- **69** offline DSP assertions pass, including:
  - 997 Hz −20 dBFS mono → −23.00 LUFS at 44.1 / 48 / 88.2 / 96 kHz file rates;
  - 96 kHz reference resampled into a 48 kHz project → −23.00 LUFS;
  - host `timeOrigin` 83.25 s → event timestamps in host time;
  - out-of-range audition rejected, not silently substituted;
  - reference player renders at the requested host position.
- `auval` passes; VST3 vendor `BerryStudio`, display version `2.0.0-beta.1`.
- Offscreen snapshots: 100 % 880×560 and 1280×820.

## Reproduce

```bash
./build/MasterLensDspTest_artefacts/Release/MasterLensDspTest
./scripts/make_dmg.sh
```

Offscreen demo (creates two temporary WAVs and analyses them):
```bash
MasterLensUiShot fileqc
```

## Honest limits

- Reference A/B plays the first reference file only; no automatic section
  alignment.
- Live coverage needs a manually entered total length; without it the reading is
  recorded as 100 % of the measured span.
- AAX / ARA2 / DDP / codec preview are not in this beta.
- The file worker is I/O bound; a very large album still takes time, but it is
  no longer tied to real-time playback.
