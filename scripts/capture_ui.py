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
    ('light', ['page0', 'light'], 'ui_shot_page0_light_211.png'),
    ('screenshot', ['page0'], 'ui_shot_page0_211.png'),
    ('spectrum', ['page2'], 'ui_shot_page2_211.png'),
    ('stereo', ['page3'], 'ui_shot_page3_211.png'),
    ('peak', ['page1'], 'ui_shot_page1_211.png'),
    ('delivery', ['page4'], 'ui_shot_page4_211.png'),
    ('checks', ['page7'], 'ui_shot_page7_211.png'),
    ('signal', ['page6'], 'ui_shot_page6_211.png'),
    ('target', ['page9'], 'ui_shot_page9_211.png'),
    ('timeline', ['page8'], 'ui_shot_page8_211.png'),
    ('english', ['page0', 'en'], 'ui_shot_en_page0_211.png'),
    ('settings', ['page0', 'settings'], 'ui_shot_page0_settings_211.png'),
    ('about', ['page0', 'about'], 'ui_shot_page0_about_211.png'),
    ('album', ['page5', 'albumcap'], 'ui_shot_page5_211album.png'),
    ('fileqc', ['page10', 'fileqc'], 'ui_shot_page10_211files.png'),
    ('file-mono', ['page10', 'fileqc', 'fileview1'], 'ui_shot_page10_211mono.png'),
    ('file-gain', ['page10', 'fileqc', 'fileview2'], 'ui_shot_page10_211gain.png'),
    ('waterfall', ['page2', 'spectrumview1'], 'ui_shot_page2_211waterfall.png'),
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
    provenance = {'version': '2.1.1', 'platform': platform.system(),
                  'architecture': platform.machine(), 'rendererSha256': digest(renderer), 'images': []}
    for name, flags, output in SHOTS:
        suffix = '211album' if name == 'album' else '211files' if name == 'fileqc' else '211mono' if name == 'file-mono' else '211gain' if name == 'file-gain' else '211waterfall' if name == 'waterfall' else '211'
        capture = ROOT / output
        dimensions = struct.unpack('>II', capture.read_bytes()[16:24]) if capture.exists() else None
        if not (args.reuse and capture.exists() and dimensions == (2240, 1400)
                and capture.stat().st_mtime >= renderer.stat().st_mtime):
            subprocess.run([str(renderer), *flags, 'size1120', f'out={suffix}'], cwd=ROOT, check=True)
        destination = ROOT / f'docs/{name}-2.1.1.png'
        shutil.copyfile(capture, destination)
        data = destination.read_bytes()
        assert data[:8] == b'\x89PNG\r\n\x1a\n'
        width, height = struct.unpack('>II', data[16:24])
        provenance['images'].append({'path': str(destination.relative_to(ROOT)),
                                     'sha256': digest(destination), 'width': width, 'height': height,
                                     'arguments': [*flags, 'size1120', f'out={suffix}']})
        print(destination.relative_to(ROOT), width, height, flush=True)
    (ROOT / 'docs/screenshots-2.1.1.json').write_text(json.dumps(provenance, indent=2) + '\n')


if __name__ == '__main__':
    main()
