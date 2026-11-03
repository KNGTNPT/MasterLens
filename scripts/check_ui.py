#!/usr/bin/env python3
"""Exercise real editor layouts at minimum size and every supported zoom step."""
import argparse
import json
import platform
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--build-dir', default='build')
args = parser.parse_args()
renderer = ROOT / args.build_dir / 'MasterLensUiShot_artefacts/Release/MasterLensUiShot'
if platform.system() == 'Darwin':
    renderer = renderer.with_name('MasterLensUiShot.app') / 'Contents/MacOS/MasterLensUiShot'
cases = []
for page in range(11):
    flags = [f'page{page}', 'minsize']
    if page == 10:
        flags += ['fileqc']
    cases.append(flags)
for scale in (100, 125, 150, 175, 200):
    for flags in (['page0'], ['page0', 'settings'], ['page10', 'fileqc', 'fileview2']):
        cases.append([*flags, 'size1120', f'scale{scale}'])
cases += [
    ['page10', 'fileqc', 'fileview1', 'minsize'],
    ['page10', 'fileqc', 'fileview2', 'minsize'],
    ['page10', 'fileqc', 'en', 'minsize'],
    ['page9', 'en', 'minsize'],
    ['page4', 'en', 'minsize'],
    ['page0', 'about', 'minsize'],
    ['page0', 'light', 'size1120'],
    ['page10', 'fileqc', 'light', 'size1120'],
    ['page10', 'empty', 'minsize'],
]
results = []
for i, flags in enumerate(cases):
    command = [str(renderer), *flags, f'out=qa232_{i:02d}']
    result = subprocess.run(command, cwd=ROOT, capture_output=True, text=True)
    results.append({'arguments': command[1:], 'returncode': result.returncode,
                    'stdout': result.stdout, 'stderr': result.stderr})
    print(f'{i+1}/{len(cases)} exit={result.returncode}: {" ".join(flags)}', flush=True)
report = ROOT / 'docs/layout-2.3.2.json'
report.write_text(json.dumps({'platform': platform.system(), 'cases': results}, indent=2) + '\n')
failures = [x for x in results if x['returncode'] != 0]
for failure in failures:
    print(failure['stderr'])
raise SystemExit(1 if failures else 0)
