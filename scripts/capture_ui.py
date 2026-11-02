#!/usr/bin/env python3
"""Capture the real editor, validate layout, and refresh README screenshots.

Build MasterLensUiShot first. On headless Linux provide DISPLAY (Xvfb) and a
Fontconfig configuration containing a Chinese font. PNGs are copied unchanged.
"""
import argparse
import hashlib
import json
import platform
import shutil
import struct
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SHOTS = [
    ('light', ['page0', 'light'], 'ui_shot_page0_light_210.png'),
    ('screenshot', ['page0'], 'ui_shot_page0_210.png'),
    ('spectrum', ['page2'], 'ui_shot_page2_210.png'),
    ('stereo', ['page3'], 'ui_shot_page3_210.png'),
    ('peak', ['page1'], 'ui_shot_page1_210.png'),
    ('delivery', ['page4'], 'ui_shot_page4_210.png'),
    ('checks', ['page7'], 'ui_shot_page7_210.png'),
    ('signal', ['page6'], 'ui_shot_page6_210.png'),
    ('target', ['page9'], 'ui_shot_page9_210.png'),
    ('timeline', ['page8'], 'ui_shot_page8_210.png'),
    ('english', ['page0', 'en'], 'ui_shot_en_page0_210.png'),
    ('settings', ['page0', 'settings'], 'ui_shot_page0_settings_210.png'),
    ('about', ['page0', 'about'], 'ui_shot_page0_about_210.png'),
    ('album', ['page5', 'albumcap'], 'ui_shot_page5_210album.png'),
    ('fileqc', ['page10', 'fileqc'], 'ui_shot_page10_210files.png'),
    ('file-mono', ['page10', 'fileqc', 'fileview1'], 'ui_shot_page10_210mono.png'),
    ('file-gain', ['page10', 'fileqc', 'fileview2'], 'ui_shot_page10_210gain.png'),
    ('waterfall', ['page2', 'spectrumview1'], 'ui_shot_page2_210waterfall.png'),
]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-dir', default='build')
    parser.add_argument('--reuse', action='store_true',
                        help='reuse captures newer than this renderer (same invocation outputs only)')
    args = parser.parse_args()
    build = (ROOT / args.build_dir).resolve()
    renderer = build / 'MasterLensUiShot_artefacts/Release/MasterLensUiShot'
    if platform.system() == 'Darwin':
        renderer = build / 'MasterLensUiShot_artefacts/Release/MasterLensUiShot.app/Contents/MacOS/MasterLensUiShot'
    renderer = renderer.resolve(strict=True)
    provenance = {'version': '2.1.0', 'platform': platform.system(),
                  'architecture': platform.machine(), 'rendererSha256': digest(renderer), 'images': []}
    for name, flags, output in SHOTS:
        suffix = '210album' if name == 'album' else '210files' if name == 'fileqc' else '210mono' if name == 'file-mono' else '210gain' if name == 'file-gain' else '210waterfall' if name == 'waterfall' else '210'
        capture = ROOT / output
        dimensions = struct.unpack('>II', capture.read_bytes()[16:24]) if capture.exists() else None
        if not (args.reuse and capture.exists() and dimensions == (2240, 1400)
                and capture.stat().st_mtime >= renderer.stat().st_mtime):
            subprocess.run([str(renderer), *flags, 'size1120', f'out={suffix}'], cwd=ROOT, check=True)
        destination = ROOT / f'docs/{name}-2.1.0.png'
        shutil.copyfile(capture, destination)
        data = destination.read_bytes()
        assert data[:8] == b'\x89PNG\r\n\x1a\n'
        width, height = struct.unpack('>II', data[16:24])
        provenance['images'].append({'path': str(destination.relative_to(ROOT)),
                                     'sha256': digest(destination), 'width': width, 'height': height,
                                     'arguments': [*flags, 'size1120', f'out={suffix}']})
        print(destination.relative_to(ROOT), width, height, flush=True)
    (ROOT / 'docs/screenshots-2.1.0.json').write_text(json.dumps(provenance, indent=2) + '\n')


if __name__ == '__main__':
    main()
