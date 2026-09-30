from pathlib import Path
import os,subprocess,json,csv,hashlib,zipfile,collections
import numpy as np
import openpyxl
R=Path(__file__).resolve().parent;W=R.parent;S=R/'SParamView';B=R/'build-native';O=R/'validation';O.mkdir(exist_ok=True)
env=os.environ.copy();env.update(QT_QPA_PLATFORM='offscreen',QT_PLUGIN_PATH=str(W/'qt-sdk/plugins'),LD_LIBRARY_PATH=str(W/'qt-sdk/lib'))
suite=[]
def run(name,args):
 try:p=subprocess.run(list(map(str,args)),capture_output=True,text=True,env=env,timeout=150)
 except PermissionError as e:
  suite.append(dict(name=name,returncode=None,blocked=str(e)));(O/'suite.json').write_text(json.dumps(suite,indent=2));print(name,'BLOCKED',flush=True);return
 (O/(name+'.log')).write_text(p.stdout+p.stderr);suite.append(dict(name=name,returncode=p.returncode))
 (O/'suite.json').write_text(json.dumps(suite,indent=2));print(name,p.returncode,p.stdout[:120],flush=True)
 assert p.returncode==0,(name,p.stderr[-2000:])
for name in ['si_tests','si_mapping_tests','si_polarity_tests','si_termination_tests','si_performance_tests','si_qt_tests']:run(name,[B/name])
for c in ['snapshot','cr_only','false_noise','valid_noise','empty_tdr','truncated_live_cache','invalid_cached_frequency','zero_transfer','cancel_reopen','parallel_import']:
 run('hardening_'+c,[B/'si_hardening_tests',c,O/'hardening'/c])
run('navigation',[B/'si_navigation_tests',O/'navigation'])
run('selftest',[B/'SParamView','--selftest',S/'examples',O/'selftest'])
paths=[W/'upload/SAFARI1_20260911_260915_112846_6008_OUTPUT_CLK.S16P',R/'verified-input/SAFARI1_20260911_260915_093510_9016_INPUT.S28P']
for p,digest in zip(paths,['556826ae4f76aeaeaa6dc4388b013a45f3f4a24147f1cb5d6a4d8d1df2947ad6','7a8db8f261a97507d54a19baca26d235919977530da83c3fade70db9b635ffdd']):
 assert hashlib.sha256(p.read_bytes()).hexdigest()==digest,'Input integrity failure: '+p.name
for mode,inputs in [('clk',[paths[0]]),('input',[paths[1]]),('warm',paths),('fallback',[]),('batch',paths)]:run('open_'+mode,[B/'si_open_tests',O/('open_'+mode),mode,*inputs])
stats=[];workflows=[]
for source in paths:
 key='CLK' if 'OUTPUT_CLK' in source.name else 'INPUT';data=O/key;old=W/'verify-safari1-20260919/evidence'/key
 run('probe_'+key,[B/'si_real_file_probe',source,O/'cache',data])
 assert (data/'matrix.bin').read_bytes()==(old/'matrix.bin').read_bytes()
 actual=list(csv.DictReader((data/'candidates.csv').open(encoding='utf-8-sig')))
 expected=list(csv.DictReader((old/'diagnostic_mapping.csv').open(encoding='utf-8-sig')))
 def ports(rows):return {r['Channel']:tuple(int(r[k] or 0) for k in ['NearP','NearN','FarP','FarN']) for r in rows}
 assert ports(actual)==ports(expected)
 conditions=0
 for reverse in [0,1]:
  direction='reverse' if reverse else 'forward';folder=data/direction
  run('tdr_'+key+'_'+direction,[B/'si_termination_probe',source,data/'candidates.csv',O/'cache',folder,reverse])
  def index(path):return {(r['channel'],r['termination']):r for r in csv.DictReader(path.open())}
  aa=index(folder/'index.csv');bb=index(old/direction/'index.csv');assert aa.keys()==bb.keys()
  for k,a in aa.items():
   b=bb[k];assert {x:v for x,v in a.items() if x!='index'}=={x:v for x,v in b.items() if x!='index'}
   assert (folder/(a['index']+'.bin')).read_bytes()==(old/direction/(b['index']+'.bin')).read_bytes()
  conditions+=len(aa)
 stats.append(dict(file=source.name,automatic_channels=len(actual),tdr_conditions_bit_identical=conditions,matrix_bit_identical=True,sha256=hashlib.sha256(source.read_bytes()).hexdigest()))
 (O/'numerical_regression.json').write_text(json.dumps(stats,indent=2))
 channels=[]
 for c in actual:
  channels.append(dict(id=c['ID'],name=c['Channel'],alias='',group=c['Group'],**{a:int(c[b] or 0)-1 for a,b in [('nearP','NearP'),('nearN','NearN'),('farP','FarP'),('farN','FarN')]}))
 project=json.loads((S/'examples/demo.siproject').read_text());project['name']='SAFARI1 '+key
 project['settings'].update(startHz=0,stopHz=20e9,targetOhm=0,tdrLimit=False,tdrTerminations=[0,1,2,3,4],manualMarkersHz=[])
 for limit in project['settings']['limits']:limit['enabled']=False
 project['revisions']=[dict(name=key,source=str(source),channels=channels,confirmed=True)]
 project['ui']={'revision':0,'selected':[c['id'] for c in channels],'group':'All groups','metric':0}
 path=data/'workflow.siproject';path.write_text(json.dumps(project,indent=2));folder=data/'workflow'
 env['SI_VALIDATION_UI']='1';run('workflow_'+key,[B/'SParamView','--validate-project',path,folder]);env.pop('SI_VALIDATION_UI')
 results=json.loads((folder/'Validation.json').read_text())
 with (data/'matrix.bin').open('rb') as h:
  n,k=map(int,np.fromfile(h,'<u8',2));f=np.fromfile(h,'<f8',k);matrix=np.fromfile(h,'<c16').reshape(n,n,k)
 lookup={c['name']:c for c in channels}
 def weights(c,far=False):
  a,b=(c['farP'],c['farN']) if far else (c['nearP'],c['nearN'])
  return [(a,1.)] if b<0 else [(a,1/np.sqrt(2)),(b,-1/np.sqrt(2))]
 checked=0;max_error=0;na=[]
 for row in results['analysis']:
  if row['metric']=='TDR':continue
  if row['worst'] is None:na.append(row);continue
  c=lookup[row['channel']];ag=lookup.get(row['aggressor'],c)
  z=sum(a*b*matrix[i,j] for i,a in weights(c,row['metric'] in ['Insertion Loss','FEXT']) for j,b in weights(ag))
  y=20*np.log10(np.maximum(abs(z),1e-15));i=np.argmin(y) if row['metric']=='Insertion Loss' else np.argmax(y)
  error=abs(float(y[i])-row['worst']);assert error<1e-9 and row['status']=='N/A',(key,row,error)
  max_error=max(max_error,error);checked+=1
 assert not na if key=='CLK' else len(na)==2 and all(x['note']=='No aggressor in selected group' for x in na)
 with zipfile.ZipFile(folder/'Analysis.xlsx') as z:
  assert z.testzip() is None;images=sum(x.startswith('xl/media/') for x in z.namelist())
 wb=openpyxl.load_workbook(folder/'Analysis.xlsx',read_only=True)
 rows=sum(sum(1 for _ in wb[sheet].rows)-1 for sheet in ['Return_Loss','Insertion_Loss','NEXT','FEXT','TDR'] if sheet in wb.sheetnames);wb.close()
 assert rows==results['results'] and abs(results['tdr_start_fraction']-.2)<1e-12
 workflows.append(dict(file=source.name,results=results['results'],metric_counts=dict(collections.Counter(x['metric'] for x in results['analysis'])),frequency_results_independently_checked=checked,max_db_error=max_error,xlsx_rows=rows,xlsx_images=images,tdr_start_fraction=results['tdr_start_fraction'],quality=dict(collections.Counter(x['quality'] for x in results['analysis'] if x['metric']=='TDR'))))
 (O/'real_workflow.json').write_text(json.dumps(workflows,indent=2));print(workflows[-1],flush=True)
print('Validation completed',len(suite),'cases',flush=True)
