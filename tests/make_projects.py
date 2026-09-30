from pathlib import Path
import json,os,copy
root=Path('real-validation');projects=root/'projects';projects.mkdir(exist_ok=True)
files=json.loads((root/'real_file_crosscheck.json').read_text())['files']
settings=copy.deepcopy(json.loads(Path('SParamView/examples/demo.siproject').read_text())['settings'])
settings.update(startHz=1e8,stopHz=1e10,targetOhm=0,tdrLimit=False,tdrStart=0,tdrStop=3e-9,manualMarkersHz=[1e9,3e9,6e9,1e10])
for l in settings['limits']:l['enabled']=False

def channels(f):
 out=[]
 for c in f['suggested_channels']:
  row=dict(id=c['ID'],name=c['Channel'],group='Signals',alias='')
  for k in ['NearP','NearN','FarP','FarN']:row[k[0].lower()+k[1:]]=int(c[k])-1 if c[k] else -1
  out.append(row)
 return out

def diff(cs):
 by={c['name']:c for c in cs};out=[]
 for name,c in by.items():
  if not name.endswith('_P') or name[:-2]+'_N' not in by:continue
  n=by[name[:-2]+'_N'];out.append(dict(id='net:'+name[:-2].lower(),name=name[:-2],group='Signals',alias='',nearP=c['nearP'],nearN=n['nearP'],farP=c['farP'],farN=n['farP']))
 return out

cases=[]
def create(key,title,pairs):
 revs=[]
 for name,f,cs in pairs:
  revs.append(dict(name=name,source=os.path.relpath(Path(f['source']).resolve(),projects.resolve()),sourceSha256=f['sha256'],channels=cs,confirmed=True))
 project=dict(format='SIAnalyzerProject',schema=1,name=title,settings=copy.deepcopy(settings),revisions=revs,ui=dict(revision=0,selected=[c['id'] for c in pairs[0][2]],group='Signals',metric=0))
 path=projects/(key+'.siproject');path.write_text(json.dumps(project,indent=2,ensure_ascii=False))
 count=sum(len(cs)*(3+2*(len(cs)-1)) for _,_,cs in pairs)
 cases.append(dict(key=key,project=str(path),expected_results=count))

outa,outb=files[4],files[6];ina,inb=files[3],files[5]
create('OUTPUT_Diff','OUTPUT · 260819-2 → 260820',[('260819-2',outa,channels(outa)),('260820',outb,channels(outb))])
common={c['id'] for c in channels(ina)} & {c['id'] for c in channels(inb)}
create('INPUT_SE','INPUT · 공통 12개 Net',[('260819-2',ina,[c for c in channels(ina) if c['id'] in common]),('260820',inb,[c for c in channels(inb) if c['id'] in common])])
create('Package_G01_SE','FCCSP Group01 · Single-ended',[('G01',files[0],channels(files[0]))])
create('Package_G02_SE','FCCSP Group02 · Single-ended',[('G02',files[2],channels(files[2]))])
create('Package_G02_Diff','FCCSP Group02 · Differential',[('G02',files[2],diff(channels(files[2])))])
create('Package_G03_Diff','FCCSP Group03 · Differential',[('G03',files[1],diff(channels(files[1])))])
(root/'cases.json').write_text(json.dumps(cases,indent=2));print(cases)
