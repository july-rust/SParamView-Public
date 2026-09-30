"""Generate actual S-parameter ASCII bytes, import/cache-read, record RSS. No sparse padding."""
import argparse, pathlib, math, subprocess, time, json, platform, threading, sys
import psutil
p=argparse.ArgumentParser();p.add_argument('--cli',required=True);p.add_argument('--gib',type=float,default=5);p.add_argument('--workdir',default='stress-work');p.add_argument('--reuse-source',action='store_true');args=p.parse_args()
root=pathlib.Path(args.workdir).resolve();root.mkdir(parents=True,exist_ok=True);cli=str(pathlib.Path(args.cli).resolve());n=128
src=root/'stress.s128p';zero='0.000000000000e+00 0.000000000000e+00 '
rows=[]
for i in range(n):
 values=[zero]*n
 values[i]='5.000000000000e-02 0.000000000000e+00 '
 values[(i+n//2)%n]='9.000000000000e-01 0.000000000000e+00 '
 rows.append((' '.join(values)+'\n').encode())
frame=b''.join(rows);points=math.ceil(args.gib*(1024**3)/len(frame));begin=time.perf_counter()
if not(args.reuse_source and src.exists()):
 with src.open('wb') as out:
  out.write(f'[Version] 2.1\n# Hz S RI R 50\n[Number of Ports] {n}\n[Number of Frequencies] {points}\n[Network Data]\n'.encode())
  for k in range(points):out.write(f'{k*1000000} '.encode());out.write(frame)
  out.write(b'[End]\n')
print(json.dumps(dict(stage='generated',bytes=src.stat().st_size,points=points,seconds=time.perf_counter()-begin)),flush=True)
results=[]
def measure(label,cmd):
 t=time.perf_counter()
 if platform.system()=='Linux':
  helper="import subprocess,resource,json,sys; r=subprocess.run(sys.argv[1:],capture_output=True,text=True); print(json.dumps(dict(returncode=r.returncode,stdout=r.stdout,stderr=r.stderr,peak_rss_bytes=resource.getrusage(resource.RUSAGE_CHILDREN).ru_maxrss*1024)))"
  child=subprocess.run([sys.executable,'-c',helper,*cmd],capture_output=True,text=True,check=True)
  r=json.loads(child.stdout)
 else:
  proc=subprocess.Popen(cmd,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True);peak=0
  while proc.poll() is None:
   try:peak=max(peak,psutil.Process(proc.pid).memory_info().rss)
   except psutil.Error:pass
   time.sleep(.05)
  stdout,stderr=proc.communicate();r=dict(peak_rss_bytes=peak,returncode=proc.returncode,stdout=stdout,stderr=stderr)
 r.update(test=label,seconds=time.perf_counter()-t)
 results.append(r);print(json.dumps(r),flush=True)
 if r['returncode']:raise RuntimeError(label+' failed')
measure('Initial import',[cli,'inspect',str(src),str(root/'cache')])
measure('Valid cache reopen',[cli,'inspect',str(src),str(root/'cache')])
measure('Single trace read with process startup',[cli,'trace',str(src),'65','1',str(root/'trace.csv'),str(root/'cache')])
report=dict(platform=platform.platform(),cpu_count=psutil.cpu_count(),source_bytes=src.stat().st_size,ports=n,points=points,results=results)
(root/'stress_result.json').write_text(json.dumps(report,indent=2))
