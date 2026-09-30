from pathlib import Path
import shutil,subprocess,re,json,struct,hashlib
R=Path(__file__).resolve().parent;W=R.parent;S=R/'SParamView';D=R/'dist/SParamView-Windows-x64';D.mkdir(parents=True,exist_ok=True)
old=W/'windows-staging/SIAnalyzer-Windows-x64'
for p in old.glob('*.dll'):shutil.copy2(p,D/p.name)
for d in ['platforms','fonts']:shutil.copytree(old/d,D/d,dirs_exist_ok=True)
for d in ['examples','docs','third_party']:shutil.copytree(S/d,D/d,dirs_exist_ok=True,ignore=shutil.ignore_patterns('*.tar.xz'))
for p in (R/'build-windows').glob('*.exe'):shutil.copy2(p,D/p.name)
for name in ['LICENSE','README_KO.md','README_FIRST.txt','Run_Windows_Verification.cmd']:shutil.copy2(S/name,D/name)
shutil.copy2(old/'qt.conf',D/'qt.conf')
pefiles={p.name.lower():p for p in D.rglob('*') if p.suffix.lower() in ['.dll','.exe']}
objdump=W/'windows-deps/dev/usr/bin/x86_64-w64-mingw32-objdump';imports={};exports={}
for name,p in pefiles.items():
 t=subprocess.check_output([str(objdump),'-p',str(p)],text=True,stderr=subprocess.PIPE);assert 'file format pei-x86-64' in t,name
 im={};dll=None
 for line in t.split('The Function Table')[0].split('The Export Tables')[0].splitlines():
  m=re.search(r'DLL Name: (\S+)',line)
  if m:dll=m[1].lower();im[dll]=[];continue
  m=re.match(r'\s*[0-9a-f]+\s+\d+\s+(\S+)\s*$',line)
  if m and dll:im[dll].append(m[1])
 imports[name]=im;block=t.split('[Ordinal/Name Pointer] Table',1)[-1] if '[Ordinal/Name Pointer] Table' in t else ''
 exports[name]=set(re.findall(r'^\s*\[\s*\d+\]\s+(\S+)',block,flags=re.M))
missing=[];external=set()
for name,im in imports.items():
 for dll,symbols in im.items():
  if dll not in pefiles:external.add(dll);continue
  for symbol in symbols:
   if symbol not in exports[dll]:missing.append(dict(file=name,dll=dll,symbol=symbol))
raw=(D/'SParamView.exe').read_bytes();pe=struct.unpack_from('<I',raw,0x3c)[0]
assert struct.unpack_from('<H',raw,pe+4)[0]==0x8664 and struct.unpack_from('<H',raw,pe+24+68)[0]==2
assert '1.0.1'.encode('utf-16le') in raw and 'IDI_ICON1'.encode('utf-16le') in raw
iconsha=hashlib.sha256((S/'assets/SParamView.ico').read_bytes()).hexdigest()
assert iconsha=='020b5a966041659fde8eb1137e92ddda3d8054f0978f3cba1b735367e7aa1525'
(R/'validation/windows_audit.json').write_text(json.dumps(dict(pe_files=len(pefiles),architecture='AMD64',subsystem='Windows GUI',missing_bundled_symbols=missing,external_dlls=sorted(external),version_resource='1.0.1',icon_sha256=iconsha,imports=imports,windows_runtime_tested=False),indent=2))
print('PE files',len(pefiles),'missing symbols',len(missing),flush=True);assert not missing,missing[:20]
