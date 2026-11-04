#!/usr/bin/env python3
"""Capture the real editor, validate layout, and refresh README screenshots.

Build MasterLensUiShot first. On headless Linux provide DISPLAY (Xvfb) and a
Fontconfig configuration containing a Chinese font. PNGs are copied unchanged.
"""
import argparse
import hashlib
import json
import platform
import re
import shutil
import struct
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
VERSION = re.search(r'kVersion\s*=\s*"([^"]+)"', (ROOT / 'Source/Version.h').read_text()).group(1)
SUFFIX = VERSION.replace('.', '')
SHOTS = [
    ('screenshot', ['page0'], 'ui_shot_page0_screenshot'),
    ('signal', ['page6'], 'ui_shot_page6_signal'),
    ('spectrum', ['page2'], 'ui_shot_page2_spectrum'),
    ('stereo', ['page3'], 'ui_shot_page3_stereo'),
    ('fileqc', ['page10', 'fileqc', 'standalone'], 'ui_shot_page10_fileqc'),
    ('about', ['page0', 'about'], 'ui_shot_page0_about_about'),
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
    provenance = {'version': VERSION, 'platform': platform.system(),
                  'architecture': platform.machine(), 'renderer': f'MasterLensUiShot {VERSION}',
                  'rendererSha256': digest(renderer), 'images': []}
    for name, flags, stem in SHOTS:
        suffix = f'{name}{SUFFIX}'
        capture = ROOT / f'{stem}{SUFFIX}.png'
        dimensions = struct.unpack('>II', capture.read_bytes()[16:24]) if capture.exists() else None
        if not (args.reuse and capture.exists() and dimensions == (2240, 1400)
                and capture.stat().st_mtime >= renderer.stat().st_mtime):
            subprocess.run([str(renderer), *flags, 'size1120', f'out={suffix}'], cwd=ROOT, check=True)
        destination = ROOT / f'docs/{name}-{VERSION}.png'
        shutil.copyfile(capture, destination)
        data = destination.read_bytes()
        assert data[:8] == b'\x89PNG\r\n\x1a\n'
        width, height = struct.unpack('>II', data[16:24])
        provenance['images'].append({'path': str(destination.relative_to(ROOT)),
                                     'sha256': digest(destination), 'width': width, 'height': height,
                                     'arguments': [*flags, 'size1120', f'out={suffix}']})
        print(destination.relative_to(ROOT), width, height, flush=True)
    (ROOT / f'docs/screenshots-{VERSION}.json').write_text(json.dumps(provenance, indent=2) + '\n')


if __name__ == '__main__':
    main()
