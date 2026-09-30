from pathlib import Path
import json,shutil,hashlib,zipfile,subprocess,sys
R=Path(__file__).resolve().parent;S=R/'SParamView';V=R/'validation';D=R/'dist/SParamView-Windows-x64';OUT=R/'deliverables';OUT.mkdir(exist_ok=True)
suite=json.loads((V/'suite.json').read_text());assert len(suite)==31 and all(x['returncode']==0 for x in suite)
audit=json.loads((V/'windows_audit.json').read_text());assert not audit['missing_bundled_symbols']
evidence=S/'docs/release-1.0.1';evidence.mkdir(exist_ok=True)
for p in V.rglob('*'):
 if not p.is_file() or p.suffix not in ['.json','.log','.csv','.png','.xlsx']:continue
 if 'cache' in p.parts or 'hardening' in p.parts:continue
 dest=evidence/p.relative_to(V);dest.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(p,dest)
scripts=S/'scripts/release-audit';scripts.mkdir(exist_ok=True)
for name in ['build_release.py','validate_release.py','stage_and_audit.py','package_release.py']:shutil.copy2(R/name,scripts/name)
readme=S/'README_KO.md';readme.write_text(readme.read_text().replace('**SParamView_Source_v1.0.0_RC1.zip**','**SParamView_Source_v1.0.1.zip**'))
shutil.copytree(S/'docs',D/'docs',dirs_exist_ok=True)
for name in ['README_KO.md','README_FIRST.txt']:shutil.copy2(S/name,D/name)
report=OUT/'SParamView_Release_1.0.1.md';shutil.copy2(S/'docs/Release_1.0.1.md',report)
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
packages=[]
for root,name in [(D,'SParamView_Windows_x64_v1.0.1.zip'),(S,'SParamView_Source_v1.0.1.zip')]:
 files=sorted(p for p in root.rglob('*') if p.is_file() and p.name!='SHA256SUMS.txt')
 manifest={str(p.relative_to(root)).replace('\\','/'):sha(p) for p in files}
 (root/'SHA256SUMS.txt').write_text(''.join(v+'  '+k+'\n' for k,v in manifest.items()))
 path=OUT/name
 with zipfile.ZipFile(path,'w',zipfile.ZIP_DEFLATED,compresslevel=6) as z:
  for p in sorted(root.rglob('*')):
   if p.is_file():z.write(p,str(Path(root.name)/p.relative_to(root)))
 with zipfile.ZipFile(path) as z:
  assert z.testzip() is None
  for relative,digest in manifest.items():assert hashlib.sha256(z.read(root.name+'/'+relative)).hexdigest()==digest,relative
 packages.append(dict(name=name,bytes=path.stat().st_size,sha256=sha(path),files_verified=len(manifest)))
 print(packages[-1],flush=True)
(OUT/'Package_Integrity.json').write_text(json.dumps(packages,indent=2))
print('Release deliverables ready',flush=True)
