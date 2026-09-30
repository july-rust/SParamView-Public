from pathlib import Path
import json,subprocess,time,hashlib,csv
import numpy as np
import skrf as rf
root=Path('real-validation');data=[]
for i,p in enumerate(sorted(Path('real-inputs').rglob('*'))):
 if not p.is_file():continue
 d=root/f'file_{i:02d}';t=time.perf_counter()
 proc=subprocess.run(['build/si_real_probe',str(p),str(root/'cache'),str(d)],capture_output=True,text=True)
 row=dict(name=p.name,source=str(p),probe=str(d),bytes=p.stat().st_size,probe_seconds=time.perf_counter()-t,returncode=proc.returncode,stderr=proc.stderr)
 if proc.returncode:
  data.append(row);print(row,flush=True);continue
 a=rf.Network(str(p));raw=(d/'matrix.bin').read_bytes();n,k=np.frombuffer(raw,dtype='<u8',count=2)
 n,k=int(n),int(k);f=np.frombuffer(raw,dtype='<f8',count=k,offset=16)
 s=np.frombuffer(raw,dtype='<c16',offset=16+k*8).reshape(n,n,k).transpose(2,0,1)
 assert s.shape==a.s.shape
 error=float(np.max(abs(s-a.s)));fe=float(np.max(abs(f-a.f)));sha=hashlib.sha256(p.read_bytes()).hexdigest()
 meta=(d/'metadata.txt').read_text();assert 'sha256='+sha in meta
 assert error<2e-10 and fe<1e-3,(p,error,fe)
 refs=np.array([float(l.split('\t')[1]) for l in meta.splitlines() if l and l[0].isdigit()]);assert np.max(abs(a.z0-refs))<1e-9
 with (d/'candidates.csv').open(encoding='utf-8-sig') as file:mapping=list(csv.DictReader(file))
 row.update(ports=n,points=k,start_hz=float(f[0]),stop_hz=float(f[-1]),complex_values_checked=int(s.size),max_complex_error=error,max_frequency_error_hz=fe,sha256=sha,reference_ohm=refs.tolist(),max_reference_error=float(np.max(abs(a.z0-refs))),suggested_channels=mapping)
 data.append(row);print({k:v for k,v in row.items() if k not in ['suggested_channels','reference_ohm']},flush=True)
(root/'real_file_crosscheck.json').write_text(json.dumps(dict(scikit_rf=rf.__version__,numpy=np.__version__,files=data),indent=2))
assert all(r['returncode']==0 for r in data)
