"""Reproducible cache-layout prototype; not a substitute for target Windows NVMe measurements."""
from pathlib import Path
import numpy as np,time,json,sys
p=Path(sys.argv[1] if len(sys.argv)>1 else 'layout-work');p.mkdir(exist_ok=True)
f,n,tile=512,128,32;rng=np.random.default_rng(105)
source=rng.normal(size=(f,n*n))+1j*rng.normal(size=(f,n*n));source=source.astype(np.complex128)
results=[]
for layout in ['frequency-major','trace-major','tiled']:
 t=time.perf_counter()
 data=source if layout=='frequency-major' else source.T if layout=='trace-major' else source.reshape(f//tile,tile,n*n).transpose(0,2,1)
 path=p/(layout+'.bin');data.tofile(path);write=time.perf_counter()-t
 mmap=np.memmap(path,dtype='complex128',mode='r',shape=data.shape)
 row=dict(layout=layout,bytes=path.stat().st_size,write_seconds=write)
 for count in [1,32,64,100]:
  picks=np.arange(count)*17;t=time.perf_counter()
  traces=mmap[:,picks].copy() if layout=='frequency-major' else mmap[picks,:].copy() if layout=='trace-major' else mmap[:,picks,:].copy()
  row[f'{count}_trace_seconds']=time.perf_counter()-t
 results.append(row);del mmap;path.unlink()
(p/'layout_benchmark.json').write_text(json.dumps(results,indent=2));print(json.dumps(results))
