#!/usr/bin/env python3
"""Fetch the unmodified EBU v5 test archive for internal validation only.
Do not upload or redistribute the audio. Reports must credit © EBU.
"""
import hashlib, pathlib, subprocess, sys, zipfile
root=pathlib.Path(sys.argv[1] if len(sys.argv)>1 else '.tools/ebu')
archive=root.with_suffix('.zip');archive.parent.mkdir(parents=True,exist_ok=True)
expected='60d022fdac47ad0be2688411be9daecbff85da994d6fa4921bba6cffab841b081d8b15d9ce284ad2253efb686463450a84a0d19cb0bad7a934546cc52dd73771'
# SHA512 published by gentoo/gentoo media-libs/libebur128/Manifest.
urls=[
 'https://tech.ebu.ch/files/live/sites/tech/files/shared/testmaterial/ebu-loudness-test-setv05.zip',
 'https://mirror.nju.edu.cn/gentoo/distfiles/de/ebu-loudness-test-setv05.zip',
 'https://mirrors.mit.edu/gentoo-distfiles/distfiles/de/ebu-loudness-test-setv05.zip']
for url in urls:
 r=subprocess.run(['curl','--fail','--location','--connect-timeout','15','--max-time','120',url,'-o',str(archive)])
 if r.returncode or not archive.exists():continue
 if hashlib.sha512(archive.read_bytes()).hexdigest()!=expected:
  print('Checksum mismatch; rejecting',url);continue
 root.mkdir(parents=True,exist_ok=True)
 with zipfile.ZipFile(archive) as z:
  for member in z.infolist():
   if not (root/member.filename).resolve().is_relative_to(root.resolve()):raise SystemExit('Unsafe archive path')
  z.extractall(root)
 pathlib.Path('ebu-archive-sha256.txt').write_text(hashlib.sha256(archive.read_bytes()).hexdigest()+'  ebu-loudness-test-setv05.zip\nSource: '+url+'\nSHA512 matched Gentoo Manifest\n© EBU; internal validation only\n')
 print('Verified EBU archive:',root);break
else:raise SystemExit('EBU download unavailable; official-material checks NOT RUN')
