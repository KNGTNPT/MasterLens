#!/usr/bin/env python3
"""Independent FFmpeg ebur128 comparison + deterministic random-buffer replay.
EBU material is an optional local-only input. Never uploads/redistributes audio.
"""
import argparse, array, hashlib, json, math, os, re, shutil, struct, subprocess, tempfile, wave
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('probe');p.add_argument('--output',default='docs/measurement-2.3.2.json');p.add_argument('--ebu-dir');a=p.parse_args()
probe=str(Path(a.probe).resolve()); ffmpeg=shutil.which('ffmpeg')
if not ffmpeg: raise SystemExit('ffmpeg is required for independent verification')
rows=[]; failures=[]
def measure(path,seed=0):
 r=subprocess.run([probe,'--measure',str(path),str(seed)],capture_output=True,text=True,check=True)
 return json.loads(r.stdout)
def independent(path):
 r=subprocess.run([ffmpeg,'-hide_banner','-nostats','-i',str(path),'-af','ebur128=peak=true','-f','null','-'],capture_output=True,text=True,check=True)
 text=r.stderr
 def last(pattern):
  found=re.findall(pattern,text,re.S)
  return float(found[-1]) if found else -100.0
 frames=re.findall(r'M:\s*([-\d.]+)\s+S:\s*([-\d.]+)',text)
 return dict(I=last(r'Integrated loudness:\s+I:\s*([-\d.]+)'), LRA=last(r'Loudness range:\s+LRA:\s*([-\d.]+)'),
             TP=last(r'True peak:\s+Peak:\s*([-\d.]+)'),M=max([float(x[0]) for x in frames] or [-100]),S=max([float(x[1]) for x in frames] or [-100]))
def compare(path,kind):
 original=measure(path); ref=independent(path); differences={}; tolerance={'I':.10,'M':.15,'S':.15,'LRA':.5,'TP':.3}
 for metric in tolerance:
  if metric!='LRA' and original[metric]<=-70 and ref[metric]<=-70: continue
  differences[metric]=round(original[metric]-ref[metric],6)
  if abs(differences[metric])>tolerance[metric]:failures.append(f'{path.name} {metric}: {differences[metric]}')
 drift=0
 for seed in (23,9871):
  replay=measure(path,seed)
  for key in original: drift=max(drift,abs(original[key]-replay[key]))
 if drift>1e-5: failures.append(f'{path.name} random buffer drift {drift}')
 rows.append(dict(file=path.name,kind=kind,sha256=hashlib.sha256(path.read_bytes()).hexdigest(),masterlens=original,ffmpeg=ref,error=differences,randomBufferMaxError=drift))
 print(path.name,differences,'random',drift,flush=True)
def write(path,rate,kind,duration):
 n=int(rate*duration); values=array.array('h')
 for i in range(n):
  t=i/rate
  if kind=='silence': l=r=0
  elif kind=='tail':l=r=.05*math.sin(2*math.pi*997*t) if i<n-1 else .95
  elif kind=='near-nyquist':l=r=.6*math.sin(2*math.pi*.45*i+.7)
  else:
   level=(-18,-32,-24,-40,-15,-25)[min(int(t/2),5)]
   amp=10**(level/20); gate=0 if 4<t<4.7 else 1
   l=gate*amp*(.7*math.sin(2*math.pi*173*t)+.2*math.sin(2*math.pi*3001*t))
   r=gate*amp*(.7*math.sin(2*math.pi*173*t+.3)+.2*math.sin(2*math.pi*3001*t))
  values.extend((int(max(-1,min(1,l))*32767),int(max(-1,min(1,r))*32767)))
 if os.sys.byteorder!='little': values.byteswap()
 with wave.open(str(path),'wb') as f:f.setparams((2,2,rate,0,'NONE','PCM'));f.writeframes(values.tobytes())
with tempfile.TemporaryDirectory(prefix='ml-measure-') as d:
 for rate in (32000,44100,48000,96000,192000):
  for kind,duration in [('dynamic',12.317),('tail',.4513),('near-nyquist',4.191),('silence',3.315)]:
   path=Path(d)/f'{kind}-{rate}.wav';write(path,rate,kind,duration);compare(path,'generated-boundary')
if a.ebu_dir:
 for path in sorted(Path(a.ebu_dir).rglob('*.wav')):
  if path.name.startswith(('seq-3341-','seq-3342-')):
   try:
    with wave.open(str(path)) as f:channels=f.getnchannels()
   except wave.Error:continue
   if channels<=2:compare(path,'EBU-standard-material')
report=dict(reference='FFmpeg ebur128 (independent of MasterLens)',ffmpegVersion=subprocess.check_output([ffmpeg,'-version'],text=True).splitlines()[0],
            standardCredit='© EBU' if a.ebu_dir else 'EBU material not supplied; generated vectors only',rows=rows,failures=failures)
Path(a.output).parent.mkdir(parents=True,exist_ok=True);Path(a.output).write_text(json.dumps(report,indent=2)+'\n')
if failures:raise SystemExit('\n'.join(failures))
