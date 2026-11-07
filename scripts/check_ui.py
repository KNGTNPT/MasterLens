#!/usr/bin/env python3
"""Exercise real editor layouts at minimum size and every supported zoom step."""
import argparse
import json
import os
import platform
import re
import subprocess
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
VERSION = re.search(r'kVersion\s*=\s*"([^"]+)"', (ROOT / 'Source/Version.h').read_text()).group(1)
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--build-dir', default='build')
parser.add_argument('--jobs', type=int, default=min(6, os.cpu_count() or 4),
                    help='parallel UiShot processes (each case is independent)')
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
    # 3.0 state machines and overlays (CI used to rely on manual clicks).
    ['page0', 'settings', 'minsize'],
    ['page0', 'settings', 'size1120', 'scale200'],
    ['page0', 'about', 'size1120', 'scale200'],
    ['page9', 'size1120', 'scale200'],
    ['page10', 'fileqc', 'standalone', 'size1120', 'scale200'],
    ['page2', 'spectrumview2', 'size1120'],
    ['page9', 'selectionset', 'size1120'],
    ['page10', 'fileqc', 'codecstate', 'standalone', 'size1120'],
    ['page10', 'fileqc', 'codecstate', 'standalone', 'minsize'],
    ['page10', 'fileqc', 'codecencoding', 'standalone', 'size1120'],
    ['page10', 'fileqc', 'codecfail', 'standalone', 'size1120'],
]
def run_case(index, flags):
    tag = f'qa{VERSION.replace(".", "")}_{index:02d}'

    # 1) Real layout assertions (component bounds, fonts, all pages / scales).
    check_command = [str(renderer), *flags, 'layoutcheck']
    check = subprocess.run(check_command, cwd=ROOT, capture_output=True, text=True)

    # 2) Render the same state so the report also proves it paints.
    command = [str(renderer), *flags, f'out={tag}']
    render = subprocess.run(command, cwd=ROOT, capture_output=True, text=True)

    state = [line for line in (render.stdout + check.stdout).splitlines()
             if line.startswith('STATE')]
    return index, flags, check, render, state


results = []
with ThreadPoolExecutor(max_workers=max(1, args.jobs)) as pool:
    futures = [pool.submit(run_case, i, flags) for i, flags in enumerate(cases)]

    for done, future in enumerate(as_completed(futures), start=1):
        index, flags, check, render, state = future.result()
        results.append({'order': index * 2, 'arguments': check.args[1:], 'mode': 'layout',
                        'returncode': check.returncode,
                        'stdout': check.stdout, 'stderr': check.stderr})
        results.append({'order': index * 2 + 1, 'arguments': render.args[1:], 'mode': 'render',
                        'returncode': render.returncode,
                        'stdout': render.stdout, 'stderr': render.stderr})
        print(f'{done}/{len(cases)} layout={check.returncode} render={render.returncode}: '
              f'{" ".join(flags)}' + (f' [{" | ".join(state)}]' if state else ''), flush=True)

results.sort(key=lambda item: item.pop('order'))
report = ROOT / f'docs/layout-{VERSION}.json'
report.write_text(json.dumps({'platform': platform.system(), 'cases': results}, indent=2) + '\n')
failures = [x for x in results if x['returncode'] != 0]
for failure in failures:
    print(failure['stderr'])
raise SystemExit(1 if failures else 0)
