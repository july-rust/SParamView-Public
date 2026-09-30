from pathlib import Path
import subprocess,os,time,json
root=Path('real-validation');env=os.environ.copy();env['QT_QPA_PLATFORM']='offscreen';env['LD_LIBRARY_PATH']=str(Path('qt-sdk/lib').resolve())+':'+str(Path('toolchains/dev/usr/lib/x86_64-linux-gnu').resolve())
results=[]
for case in json.loads((root/'cases.json').read_text()):
 dest=root/'gui'/case['key'];t=time.perf_counter()
 r=subprocess.run(['build/SParamView','--validate-project',str(Path(case['project']).resolve()),str(dest.resolve())],env=env,capture_output=True,text=True,timeout=600)
 (root/(case['key']+'.log')).write_text(r.stdout+r.stderr)
 row=dict(case=case['key'],returncode=r.returncode,seconds=time.perf_counter()-t);print(row,flush=True);results.append(row)
 assert r.returncode==0,r.stderr
 j=json.loads((dest/'Validation.json').read_text());assert j['results']==case['expected_results'],(case,j['results'])
(root/'gui_runs.json').write_text(json.dumps(results,indent=2))
