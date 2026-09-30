"""Independent numerical cross-check using scikit-rf, NumPy and SciPy."""
from pathlib import Path
import sys, subprocess, tempfile, json
import numpy as np
import skrf as rf
from scipy.integrate import cumulative_trapezoid
from scipy.signal.windows import kaiser
cli=str(Path(sys.argv[1]).resolve())
root=Path(sys.argv[2] if len(sys.argv)>2 else tempfile.mkdtemp(prefix='si-validation-'))
root.mkdir(parents=True,exist_ok=True)
rng=np.random.default_rng(72)
checks=[]
reference_discrepancies=[]
def run(*args):return subprocess.run([cli,*map(str,args)],text=True,capture_output=True,check=True)
def compare_trace(p,i,j,wanted,label):
 out=root/'trace.csv';run('trace',p,i+1,j+1,out,root/'cache')
 a=np.loadtxt(out,delimiter=',',skiprows=1);a=np.atleast_2d(a)
 actual=a[:,1]+1j*a[:,2]
 err=float(np.max(np.abs(actual-wanted)))
 assert err<2e-10,(label,err)
 checks.append(dict(test=label,max_abs_error=err))
for n in [1,2,3,4,8]:
 for fmt in ['RI','MA','DB']:
  f=np.array([1e6,2e6,5e6,9e6])
  s=(rng.normal(size=(4,n,n))+1j*rng.normal(size=(4,n,n)))*.12
  for matrix in ['Full','Lower','Upper']:
   if matrix!='Full':s=(s+s.transpose(0,2,1))/2
   for order in (['12_21','21_12'] if n==2 else ['12_21']):
    path=root/f'n{n}_{fmt}_{matrix}_{order}.s{n}p'
    with path.open('w') as out:
     out.write(f'[Version] 2.0\n# kHz S {fmt} R 50\n[Number of Ports] {n}\n')
     if n==2:out.write(f'[Two-Port Data Order] {order}\n')
     out.write(f'[Number of Frequencies] 4\n[Matrix Format] {matrix}\n[Network Data]\n')
     for k in range(4):
      pairs=[(i,j) for i in range(n) for j in range(n) if matrix=='Full' or (matrix=='Lower' and i>=j) or (matrix=='Upper' and i<=j)]
      if n==2 and matrix=='Full' and order=='21_12':pairs=[(0,0),(1,0),(0,1),(1,1)]
      out.write(f'{f[k]/1e3:.16g} ')
      for count,(i,j) in enumerate(pairs):
       z=s[k,i,j]
       a,b=(z.real,z.imag) if fmt=='RI' else (abs(z) if fmt=='MA' else 20*np.log10(abs(z)),np.angle(z,deg=True))
       out.write(f'{a:.16g} {b:.16g} ')
       if count%3==2:out.write('\n')
      out.write('\n')
     out.write('[End]\n')
    reference=rf.Network(str(path))
    if np.max(np.abs(reference.s-s))>=1e-12:
     assert n==2 and matrix!='Full' and order=='21_12',path.name
     reference_discrepancies.append(dict(file=path.name,reason='scikit-rf 2.1.0 drops triangular off-diagonals for 21_12; IBIS 2.1 p15 and analytic fixture are authoritative'))
     expected=s
    else:expected=reference.s
    for i,j in {(0,0),(n-1,0),(0,n-1),(n-1,n-1)}:compare_trace(path,i,j,expected[:,i,j],path.stem+f' S{i+1}{j+1}')
# Native mixed-mode ordering to physical S and physical-to-differential extraction.
n=4;f=np.arange(9)*1e7;s=(rng.normal(size=(9,n,n))+1j*rng.normal(size=(9,n,n)))*.05
M=np.array([[0,0,1,-1],[1,1,0,0],[1,-1,0,0],[0,0,1,1]])/np.sqrt(2)
sm=np.einsum('ij,fjk,lk->fil',M,s,M)
p=root/'mixed.s4p'
with p.open('w') as out:
 out.write('[Version] 2.1\n# Hz S RI R 50\n[Number of Ports] 4\n[Number of Frequencies] 9\n[Mixed-Mode Order] D3,4 C1,2 D1,2 C3,4\n[Network Data]\n')
 for k in range(9):out.write(str(f[k])+' '+' '.join(f'{z.real:.17g} {z.imag:.17g}' for z in sm[k].flat)+'\n')
 out.write('[End]\n')
for i in range(n):
 for j in range(n):compare_trace(p,i,j,s[:,i,j],f'mixed physical S{i+1}{j+1}')
# Known TDR networks compared with scikit-rf's independent irfft and step integration.
f=np.linspace(0,10e9,513);w=2*np.pi*f;refz=50
networks={'ideal':np.zeros_like(f,dtype=complex),'step75':np.full_like(f,.2,dtype=complex),'delayed_step':.1*np.exp(-1j*w*1.2e-9),'capacitive':-1j*w*50*.5e-12/(2+1j*w*50*.5e-12),'inductive':1j*w*.8e-9/(100+1j*w*.8e-9)}
for name,s in networks.items():
 p=root/(name+'.s1p')
 with p.open('w') as out:
  out.write('# Hz S RI R 50\n')
  for a,b in zip(f,s):out.write(f'{a:.17g} {b.real:.17g} {b.imag:.17g}\n')
 out=root/(name+'_tdr.csv');run('channel',p,1,0,0,0,'TDR',out)
 actual=np.loadtxt(out,delimiter=',',skiprows=1,encoding='utf-8-sig',usecols=(0,1))
 ntwk=rf.Network(frequency=rf.Frequency.from_f(f,unit='Hz'),s=s[:,None,None],z0=50)
 # scikit-rf accepts an explicit window callable; use the exact documented SI window.
 exact_window=lambda length: np.concatenate([np.zeros(length-513),kaiser(1025,6)[512:]])
 reference_time,rho=ntwk.step_response(window=exact_window,n=1024)
 tt=np.fft.fftshift(np.fft.fftfreq(1024,d=f[1]-f[0]))
 # scikit-rf 2.1.0 uses inclusive endpoints for even-N lowpass time labels.
 # Compare impulse/step samples by FFT index and use the physical DFT time grid.
 impedance=50*(1+rho)/(1-rho)
 mask=(tt>=0)&(tt<=min(3e-9,.45/(f[1]-f[0])))
 assert len(tt[mask])==len(actual),(name,len(tt[mask]),len(actual),tt[mask][-1],actual[-1,0])
 err=float(np.max(np.abs(impedance[mask]-actual[:,1])))
 assert err<1e-7,(name,err)
 assert np.max(np.abs(tt[mask]-actual[:,0]))<1e-18
 checks.append(dict(test='TDR vs scikit-rf '+name,max_abs_error_ohm=err,time_axis='DFT grid; scikit-rf inclusive-endpoint labels differ for even N'))
# Unicode paths; malformed and cancellation tested separately by native suite.
u=root/'한글_회로.s1p';u.write_text('# MHz S MA R 50\n1 .1 90\n2 .2 -90\n',encoding='utf-8')
compare_trace(u,0,0,np.array([.1j,-.2j]),'Unicode path')
report=dict(scikit_rf=rf.__version__,numpy=np.__version__,checks=len(checks),results=checks,reference_discrepancies=reference_discrepancies)
(root/'cross_validation.json').write_text(json.dumps(report,indent=2,ensure_ascii=False),encoding='utf-8')
print(json.dumps({k:v for k,v in report.items() if k!='results'}))
