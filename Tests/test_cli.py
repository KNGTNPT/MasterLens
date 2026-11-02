#!/usr/bin/env python3
"""End-to-end file QC: analytic audio, schema, content hashes and process exits."""
import csv
import hashlib
import io
import json
import math
import os
import struct
import subprocess
import sys
import tempfile
import wave
from pathlib import Path

cli = str(Path(sys.argv[1]).resolve())
checks = 0


def check(condition, message):
    global checks
    assert condition, message
    checks += 1


def run(*args, code=0):
    result = subprocess.run([cli, 'analyze', *map(str, args)], capture_output=True, text=True)
    check(result.returncode == code, f'exit {result.returncode} != {code}: {args}\n{result.stderr}')
    return result


def write(path, rate, inverted=False, gap=False, channels=2):
    data = bytearray()
    for i in range(rate * 2):
        envelope = min(1, i / (rate * .02), (rate * 2 - 1 - i) / (rate * .02))
        value = int(8000 * envelope * math.sin(2 * math.pi * 1000 * i / rate))
        if gap and rate <= i < rate + int(rate * .02):
            value = 0
        for ch in range(channels):
            data.extend(struct.pack('<h', -value if inverted and ch == 1 else value))
    with wave.open(str(path), 'wb') as f:
        f.setparams((channels, 2, rate, 0, 'NONE', 'not compressed'))
        f.writeframes(data)


with tempfile.TemporaryDirectory(prefix='masterlens-qc-') as temp:
    root = Path(temp)
    for rate in (44100, 48000, 96000):
        normal = root / f'normal-{rate}.wav'
        anti = root / f'anti-{rate}.wav'
        write(normal, rate)
        write(anti, rate, inverted=True)
        report = root / 'report.json'
        run(normal, anti, '--json', report)
        result = json.loads(report.read_text())
        by_name = {item['file']: item for item in result['files']}
        a, b = by_name[normal.name], by_name[anti.name]
        check(result['schemaVersion'] == 3, 'versioned QC schema')
        check(a['forensic']['sha256'] == hashlib.sha256(normal.read_bytes()).hexdigest(), 'source SHA256')
        check(abs(a['stats']['integratedLufs'] - b['stats']['integratedLufs']) < .01, 'polarity does not change loudness')
        check(max(abs(x-y) for x, y in zip(a['bandsDb'], b['bandsDb'])) < .01, 'antiphase energy must not disappear')
        check(max(x['monoLossDb'] for x in a['forensic']['bands'] if x['available']) < .01, 'dual mono retains energy')
        check(max(x['monoLossDb'] for x in b['forensic']['bands'] if x['available']) > 59, 'antiphase cancellation')
        check(a['gainBudget']['predictedTpDbtp'] <= -1 + .001, 'gain budget respects ceiling')
        run(anti, '--fail-on', 'warning', code=2)
    # Windows forbids literal quotes in filenames; a comma still requires CSV quoting.
    quoted = root / ('a,quoted.wav' if os.name == 'nt' else 'a,"quoted".wav')
    write(quoted, 48000, gap=True)
    output = run(quoted, '--csv', '--json', root / 'quoted.json')
    rows = list(csv.DictReader(io.StringIO(output.stdout)))
    check(len(rows) == 1 and rows[0]['file'] == quoted.name, 'CSV quoting / clean stdout')
    q = json.loads((root / 'quoted.json').read_text())['files'][0]['forensic']
    event = next(x for x in q['findings'] if x['code'] == 'internal_digital_silence')
    check(event['startSample'] in (48000, 47999), 'gap starts at a sample address')
    check(event['lengthSamples'] >= 960, 'gap duration preserved')
    run(quoted, '--fail-on', 'warning', code=2)
    run(quoted, '--json', root / 'missing' / 'report.json', code=1)
    run(quoted, root / 'typo.wav', code=1)
    run(quoted, '--json', quoted, code=1)
    run(quoted, '--target', 'nan', code=1)
    run(quoted, '--ceiling', '1', code=1)
    run(quoted, '--fail-on', 'typo', code=1)
    multi = root / 'three-channel.wav'
    write(multi, 48000, channels=3)
    run(multi, code=1)
    silent = root / 'silent.wav'
    with wave.open(str(silent), 'wb') as f:
        f.setparams((2, 2, 48000, 0, 'NONE', 'not compressed'))
        f.writeframes(bytes(48000 * 4))
    run(silent, '--fail-on', 'error', code=2)
    broken = root / 'broken.wav'
    broken.write_bytes(b'not a WAV file')
    run(broken, code=1)
print(f'{checks} CLI integration checks passed')
