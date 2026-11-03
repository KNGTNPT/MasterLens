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
  return float(found[-1]) if found and found[-1] != "-" and math.isfinite(float(found[-1])) else -100.0
 frames=re.findall(r'M:\s*([-\d.]+)\s+S:\s*([-\d.]+)',text)
 result=dict(I=last(r'Integrated loudness:\s+I:\s*([-\d.]+)'), LRA=last(r'Loudness range:\s+LRA:\s*([-\d.]+)'),
             TP=last(r'True peak:\s+Peak:\s*([-\d.]+)'),M=max([float(x[0]) for x in frames] or [-100]),S=max([float(x[1]) for x in frames] or [-100]))
 # ebur128 emits 100 ms frames: a short final tail can be absent from its
 # peak summary. Independently oversample the COMPLETE file and include raw
 # sample peak, so an EOF impulse is tested rather than waived as a mismatch.
 with wave.open(str(path)) as wav: rate=wav.getframerate()
 astats=subprocess.run([ffmpeg,'-hide_banner','-nostats','-i',str(path),'-af',f'apad=pad_dur=0.1,aformat=sample_fmts=dbl,aresample={rate*4}:osf=dbl,astats=reset=0','-f','null','-'],capture_output=True,text=True,check=True).stderr
 peaks=re.findall(r'Peak level dB:\s*(-?(?:\d+(?:\.\d*)?|inf))',astats)
 finite=[float(x) for x in peaks if math.isfinite(float(x))]
 if finite: result['TP']=max(result['TP'],max(finite))
 return result
def standard_targets(name):
 # EBU Tech 3341/3342 v5 sequences, corroborated by libebur128/test/tests.c.
 targets={}
 if re.match(r'seq-3341-[1-5]-',name): targets['I']=(-33.0 if name.startswith('seq-3341-2-') else -23.0,.1,.1)
 if name.startswith(('seq-3341-7_', 'seq-3341-2011-8_')):
  targets['I']=(-23.0,.1,.1);targets['LRA']=(5.0 if name.startswith('seq-3341-7_') else 15.0,1,1)
 m=re.match(r'seq-3342-([1-4])-',name)
 if m: targets['LRA']=((10,5,20,15)[int(m[1])-1],1,1)
 if name.startswith('seq-3341-13-'):targets['M']=(-23.0,.1,.1)
 if name.startswith('seq-3341-10-'):targets['S']=(-23.0,.1,.1)
 m=re.match(r'seq-3341-(1[5-9]|2[0-3])-',name)
 if m:targets['TP']=(-6.0 if int(m[1])<19 else 3.0 if int(m[1])==19 else 0.0,.4,.2)
 return targets
def compare(path,kind):
 original=measure(path); ref=independent(path); differences={}; targets=standard_targets(path.name) if kind=='EBU-standard-material' else {}; tolerance={'I':.10,'M':.15,'S':.15,'LRA':.5,'TP':.3}
 for metric in tolerance:
  if metric!='LRA' and original[metric]<=-70 and ref[metric]<=-70: continue
  differences[metric]=round(original[metric]-ref[metric],6)
  if kind!='EBU-standard-material' and abs(differences[metric])>tolerance[metric]:failures.append(f'{path.name} {metric}: {differences[metric]}')
 nominal={}
 for metric,(expected,below,above) in targets.items():
  error=original[metric]-expected
  nominal[metric]=dict(expected=expected,error=round(error,6),toleranceBelow=below,toleranceAbove=above)
  if not -below-1e-5<=error<=above+1e-5:failures.append(f'{path.name} normative {metric}: {error}')
 if path.name.startswith("calibration"):
  for metric in ("I","M","S"):
   if abs(original[metric]+23)>0.1:failures.append(f"normative calibration {metric}: {original[metric]}")
 drift=0
 for seed in (23,9871):
  replay=measure(path,seed)
  for key in original: drift=max(drift,abs(original[key]-replay[key]))
 if drift>1e-5: failures.append(f'{path.name} random buffer drift {drift}')
 rows.append(dict(file=path.name,kind=kind,sha256=hashlib.sha256(path.read_bytes()).hexdigest(),masterlens=original,ffmpeg=ref,error=differences,nominal=nominal,randomBufferMaxError=drift))
 print(path.name,differences,'random',drift,flush=True)
def write(path,rate,kind,duration):
 n=int(rate*duration); values=array.array('h')
 for i in range(n):
  t=i/rate
  if kind=='silence': l=r=0
  elif kind=='calibration': l=r=10**(-23/20)*math.sin(2*math.pi*997*t)
  elif kind=='tail':l=r=.05*math.sin(2*math.pi*997*t) if i<n-1 else .95
  elif kind=='near-nyquist':l=r=.6*min(1,i/(rate*.01),(n-1-i)/(rate*.01))*math.sin(2*math.pi*.45*i+.7)
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
  for kind,duration in [('calibration',9.317),('dynamic',12.317),('tail',.4513),('near-nyquist',4.191),('silence',3.315)]:
   path=Path(d)/f'{kind}-{rate}.wav';write(path,rate,kind,duration);compare(path,'generated-boundary')
if a.ebu_dir:
 for path in sorted(Path(a.ebu_dir).rglob('*.wav')):
  if path.name.startswith(('seq-3341-','seq-3342-')):
   try:
    with wave.open(str(path)) as f:channels=f.getnchannels()
   except wave.Error:continue
   if channels<=2:compare(path,'EBU-standard-material')
if a.ebu_dir and sum(r['kind']=='EBU-standard-material' for r in rows)<10: failures.append('insufficient EBU standard material coverage')
report=dict(reference='FFmpeg ebur128 (independent of MasterLens)',ffmpegVersion=subprocess.check_output([ffmpeg,'-version'],text=True).splitlines()[0],
            standardCredit='© EBU' if a.ebu_dir else 'EBU material not supplied; generated vectors only',rows=rows,failures=failures)
Path(a.output).parent.mkdir(parents=True,exist_ok=True);Path(a.output).write_text(json.dumps(report,indent=2)+'\n')
if failures:raise SystemExit('\n'.join(failures))
