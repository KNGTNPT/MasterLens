#!/usr/bin/env python3
"""End-to-end file QC: analytic audio, schema, content hashes and process exits."""
import csv
import hashlib
import io
import json
import math
import os
import shutil
import struct
import subprocess
import sys
import tempfile
import wave
from pathlib import Path

cli = str(Path(sys.argv[1]).resolve())
checks = 0
skipped = 0


def check(condition, message):
    global checks
    assert condition, message
    checks += 1


def skip(message):
    global skipped
    skipped += 1
    print(f'SKIP: {message}', file=sys.stderr)


def runtime_env():
    """Explicit runtime for development/CI; the release package resolves its own."""
    env = dict(os.environ)
    if not env.get('MASTERLENS_FFMPEG'):
        found = shutil.which('ffmpeg')
        if found:
            env['MASTERLENS_FFMPEG'] = found
    return env


def runtime_encoders():
    exe = runtime_env().get('MASTERLENS_FFMPEG')
    if not exe:
        return set()
    try:
        out = subprocess.run([exe, '-hide_banner', '-encoders'],
                             capture_output=True, text=True, encoding='utf-8',
                             errors='replace', timeout=10).stdout
    except Exception:
        return set()
    caps = set()
    if ' aac ' in out:
        caps.add('aac')
    if ' libmp3lame ' in out:
        caps.add('mp3')
    return caps


def run(*args, code=0, env=None):
    result = subprocess.run([cli, 'analyze', *map(str, args)], capture_output=True,
                            text=True, encoding='utf-8', errors='replace', env=env)
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
    # Strict delivery specs: end-to-end, not just direct evaluateSpec calls.
    spec = root / 'spec.json'
    measured = json.loads((root / 'quoted.json').read_text())['files'][0]['stats']['integratedLufs']
    spec.write_text(json.dumps(dict(targetLufs=measured, ceilingDbtp=-1, loudnessToleranceLu=.2, sampleRate=48000,bitDepth=16,channels=2)))
    run(quoted, '--spec', spec, '--json', root/'spec-pass.json')
    checked=json.loads((root/'spec-pass.json').read_text())
    check(checked['analysisOk'] and not checked['gateFailed'], 'spec pass is reported accurately')
    bad=json.loads(spec.read_text());bad['bitDepth']=24;spec.write_text(json.dumps(bad))
    run(quoted, '--spec', spec, '--json', root/'spec-fail.json',code=2)
    check(json.loads((root/'spec-fail.json').read_text())['gateFailed'], 'spec failure gates even without --fail-on')
    for invalid in ({'sampleRate':'48000'},{'loudnessToleranceLu':-1},{'channels':7},{'sampleRate':48000.5}):
        spec.write_text(json.dumps(invalid));run(quoted,'--spec',spec,code=1)
    spec.write_text('{}');run(silent,'--spec',spec,code=2)
    # Failures must affect the process exit and structured result.
    # Explicit but unusable runtime: fail, never fall back to PATH.
    missing_env=dict(os.environ,MASTERLENS_FFMPEG=str(root/'absent-ffmpeg'))
    run(quoted,'--codec','aac','--json',root/'codec-missing.json',code=1,env=missing_env)
    error_report=json.loads((root/'codec-missing.json').read_text())
    check(not error_report['analysisOk'] and error_report['gateFailed'] and not error_report['codecChecks'][0]['ok'],'codec failure is never a success')
    check('ffmpeg unavailable' in error_report['codecChecks'][0]['error'],'missing runtime is reported explicitly')
    # A hung encoder must fail promptly, with no successful evidence left behind.
    if os.name != 'nt':
        fake=root/'hung-ffmpeg'
        fake.write_text('''#!/bin/sh
for a in "$@"; do
  if [ "$a" = "-version" ]; then
    echo "ffmpeg version 0.0-test"; echo "configuration: --disable-gpl"; exit 0
  fi
  if [ "$a" = "-encoders" ]; then
    echo " A....D aac native AAC encoder"; echo " A....D libmp3lame libmp3lame MP3"; exit 0
  fi
done
exec sleep 10
''')
        fake.chmod(0o755)
        timeout_env=dict(os.environ,MASTERLENS_FFMPEG=str(fake),MASTERLENS_CODEC_TIMEOUT_MS='100')
        run(quoted,'--codec','aac','--json',root/'codec-timeout.json',code=1,env=timeout_env)
        timeout_report=json.loads((root/'codec-timeout.json').read_text())
        check(not timeout_report['analysisOk'],'encoder timeout cannot pass')
        check('timeout' in timeout_report['codecChecks'][0]['error'],'timeout is classified')
    original=quoted.read_bytes();run(quoted,'--json',quoted,code=1)
    check(quoted.read_bytes()==original,'report export cannot overwrite input audio')
    # Real encoded packets -> float WAV -> analysis, through the shared runtime.
    caps=runtime_encoders()
    require = os.environ.get('MASTERLENS_REQUIRE_RUNTIME') == '1'
    if not caps:
        if require:
            raise AssertionError('bundled ffmpeg runtime required for codec round-trip coverage')
        skip('ffmpeg runtime unavailable; codec round-trip not covered')
    else:
        codec_env=runtime_env()
        for codec in ('aac','mp3'):
            if codec not in caps:
                skip(f'{codec} encoder missing from this runtime')
                continue
            report=root/f'codec-{codec}.json';run(quoted,'--codec',codec,'--json',report,env=codec_env)
            cc=json.loads(report.read_text())['codecChecks'][0]
            check(cc['ok'] and cc['encodedBytes']>100 and cc['encodedContainer'] in ('m4a','mp3'),'real codec container and packets')
            check(len(cc['encodedSha256'])==64 and len(cc['decodedSha256'])==64,'encoded and decoded content fingerprints')
            check(cc['integratedLufs']>-70 and cc['truePeakDbtp']>-90,'decoded measurements are valid')
            check(not cc['decodedPath'],'temporary decoded audio is not leaked as a durable evidence path')
            check(cc['backend'] in ('bundled','override','system'),'backend is recorded in the report')
        # Unicode / spaces path, mono 44.1 kHz short file.
        unicode_dir=root/'素材 文件夹';unicode_dir.mkdir()
        short=unicode_dir/'短 测试.wav';write(short,44100,channels=1)
        report=root/'codec-unicode.json';run(short,'--codec','aac','--json',report,env=codec_env)
        cc=json.loads(report.read_text())['codecChecks'][0]
        check(cc['ok'],'codec works with unicode / spaces / mono / 44.1 kHz')
        # Corrupt input must fail even when the runtime is healthy.
        corrupt=root/'corrupt-codec.wav';corrupt.write_bytes(b'not a wav')
        run(corrupt,'--codec','aac','--json',root/'codec-corrupt.json',code=1,env=codec_env)
        corrupt_checks=json.loads((root/'codec-corrupt.json').read_text())['codecChecks']
        check(not corrupt_checks or not corrupt_checks[0]['ok'],
              'corrupt input cannot pass codec coverage')
print(f'{checks} CLI integration checks passed, {skipped} skipped')
