from pathlib import Path
import json,zipfile,io,hashlib
from PIL import Image
import openpyxl
root=Path('real-validation');records=[]
for case in json.loads((root/'cases.json').read_text()):
 d=root/'gui'/case['key'];j=json.loads((d/'Validation.json').read_text())
 assert j['application']=='0.1.2-preview.1'
 count=0
 for p in d.glob('*.png'):
  with Image.open(p) as im:im.load();assert im.width>=1000
  count+=1
 with zipfile.ZipFile(d/'Analysis.xlsx') as z:
  assert z.testzip() is None
  images=[n for n in z.namelist() if n.startswith('xl/media/')]
  for n in images:
   with Image.open(io.BytesIO(z.read(n))) as im:im.load()
 wb=openpyxl.load_workbook(d/'Analysis.xlsx',read_only=True,data_only=False)
 directions={'Return_Loss':'Near -> Near','Insertion_Loss':'Near -> Far','NEXT':'Aggressor Near -> Victim Near','FEXT':'Aggressor Near -> Victim Far','TDR':'Near -> Near'}
 total=0
 for name,direction in directions.items():
  rows=wb[name].iter_rows();headers=[c.value for c in next(rows)]
  for cells in rows:
   row=dict(zip(headers,cells));total+=1;assert row['Direction'].value==direction
   value=row['Worst value']
   if isinstance(value.value,(float,int)):assert value.data_type=='n'
   elif name=='TDR':assert row['TDR quality'].value=='UNSUITABLE'
   if name=='TDR':assert row['Metric'].value.startswith('Impedance (from ')
   else:assert row['Metric'].value.endswith(' [dB]')
 assert total==case['expected_results'];wb.close()
 records.append(dict(case=case['key'],standalone_png=count,embedded_png=len(images),detail_rows=total))
 print(records[-1],flush=True)
files=json.loads((root/'real_file_crosscheck.json').read_text())['files'];integrity=[]
for f in files:
 digest=hashlib.sha256(Path(f['source']).read_bytes()).hexdigest();assert digest==f['sha256']
 integrity.append(dict(file=f['name'],sha256=digest,unchanged=True))
Path('review-validation/output_integrity.json').write_text(json.dumps(dict(cases=records,source_integrity=integrity),indent=2))
print('PASS: all PNG images, numeric cells, directions and original hashes')
