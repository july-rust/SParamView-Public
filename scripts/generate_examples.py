"""Deterministic synthetic fixtures. These are not measured PCB datasets."""
from pathlib import Path
import json, math, sys
root=Path(sys.argv[1]) if len(sys.argv)>1 else Path(__file__).resolve().parents[1]/'examples'
root.mkdir(parents=True,exist_ok=True)
channels=[]
for i,name in enumerate(['CLK','DATA0','DATA1','DATA2']):
 channels.append(dict(id='demo:'+name,name=name,alias='',group='MIPI',nearP=2*i,nearN=2*i+1,farP=8+2*i,farN=9+2*i))
for rev,points in [('REV_A',401),('REV_B',451)]:
 with (root/(rev+'.s16p')).open('w',encoding='ascii') as out:
  out.write(f'! SYNTHETIC demonstration only; not a measured board\n[Version] 2.1\n# GHz S RI R 50\n[Number of Ports] 16\n[Number of Frequencies] {points}\n')
  for ch in channels:
   for field,label in [('nearP','Near_P'),('nearN','Near_N'),('farP','Far_P'),('farN','Far_N')]:out.write(f'! Port[{ch[field]+1}] = {ch["name"]}_{label}\n')
  out.write('[Network Data]\n')
  for k in range(points):
   f=10*k/(points-1)
   s=[[0j]*16 for _ in range(16)]
   def diff(response,stimulus,z):
    # response/stimulus differential endpoint: 0..3 near, 4..7 far
    rp=2*(response%4)+(8 if response>=4 else 0)
    sp=2*(stimulus%4)+(8 if stimulus>=4 else 0)
    for a,signa in [(rp,1),(rp+1,-1)]:
     for b,signb in [(sp,1),(sp+1,-1)]:s[a][b]+=z*signa*signb/2
   for i in range(4):
    center=7.1+.24*i
    rl=-24+ (12+1.3*i)*math.exp(-((f-center)/.55)**2)+1.2*math.sin(f*1.8+i)
    if rev=='REV_B':rl-=3.8
    z=10**(rl/20)*complex(math.cos(-2*math.pi*f*.42),math.sin(-2*math.pi*f*.42))
    diff(i,i,z);diff(i+4,i+4,z*.85)
    il=-.1-(.23+.016*i)*f-(.35 if rev=='REV_A' else .10)*math.exp(-((f-8.7)/.32)**2)
    z=10**(il/20)*complex(math.cos(-2*math.pi*f*.28),math.sin(-2*math.pi*f*.28))
    diff(i+4,i,z);diff(i,i+4,z)
    for j in range(4):
     if i==j:continue
     next_db=-43+7*math.exp(-((f-6.3)/.65)**2)+(3 if i==2 else 0)
     fext_db=-42+(14 if i==3 and j==1 else 6)*math.exp(-((f-8.14)/.42)**2)
     if rev=='REV_B':next_db-=3;fext_db-=5
     diff(i,j,10**(next_db/20)*complex(math.cos(-f*.8),math.sin(-f*.8)))
     diff(i+4,j,10**(fext_db/20)*complex(math.cos(-f),math.sin(-f)))
   out.write(f'{f:.12g} ')
   for i,row in enumerate(s):
    if i:out.write('  ')
    out.write(' '.join(f'{z.real:.12g} {z.imag:.12g}' for z in row)+'\n')
  out.write('[End]\n')
settings=dict(startHz=1e8,stopHz=9e9,reverse=False,markers=3,prominence=.15,targetOhm=100,tdrLimit=True,tolerancePercent=10,tdrStart=0,tdrStop=3e-9,kaiserBeta=6,performance=1,manualMarkersHz=[1e9,3e9,6e9,9e9],quick=[0,1,2,3,4],limits=[dict(enabled=True,constant=v,points=[]) for v in [-10,-3,-30,-30]])
project=dict(format='SIAnalyzerProject',schema=1,name='MIPI · Synthetic Demo',settings=settings,revisions=[dict(name=r,source=r+'.s16p',channels=channels,confirmed=True) for r in ['REV_A','REV_B']],ui=dict(revision=0,selected=[c['id'] for c in channels],group='MIPI',metric=0))
(root/'demo.siproject').write_text(json.dumps(project,ensure_ascii=False,indent=2),encoding='utf-8')
with (root/'mapping.csv').open('w',encoding='utf-8-sig') as out:
 out.write('Channel,NearP,NearN,FarP,FarN,Group,Alias,ID\n')
 for c in channels:out.write(','.join([c['name']]+[str(c[x]+1) for x in ['nearP','nearN','farP','farN']]+['MIPI','',c['id']])+'\n')
(root/'README.txt').write_text('Synthetic demonstration only. Limits are explicit example rules, not MIPI standards. Open demo.siproject, then click QUICK ANALYSIS.\n',encoding='utf-8')
print(root)
