from pathlib import Path
import concurrent.futures as cf
import os,subprocess,sys,json,shutil
ROOT=Path(__file__).resolve().parent;WS=ROOT.parent;SRC=ROOT/'SParamView'
target=sys.argv[1];win=target=='windows';out=ROOT/('build-'+target);out.mkdir(exist_ok=True)
qt=WS/('qt-win-sdk' if win else 'qt-sdk')
compiler=str(WS/'windows-deps/dev/usr/bin/x86_64-w64-mingw32-g++-posix') if win else 'g++'
driver=['-B'+str(WS/'windows-deps/dev/usr/bin/x86_64-w64-mingw32-')] if win else []
flags=['-std=c++20','-O2','-pthread','-I'+str(SRC/'include'),'-I'+str(SRC/'src'),'-DQT_NO_DEBUG',*driver]
if not win:flags+=['-fPIC','-I'+str(WS/'deps/dev/usr/include')]
for part in ['','QtWidgets','QtGui','QtCore','QtConcurrent']:flags+=['-I'+str(qt/'include'/part)]
flags+=['-I'+str(qt/'mkspecs'/('win32-g++' if win else 'linux-g++'))]
env=os.environ.copy()
if win:
 env['PATH']=str(WS/'windows-deps/dev/usr/bin')+os.pathsep+env['PATH']
 flags+=['-idirafter',str(WS/'windows-deps/dev/usr/share/mingw-w64/include')]
def run(cmd,label):
 p=subprocess.run(list(map(str,cmd)),stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,env=env)
 (out/(label+'.log')).write_text(p.stdout)
 if p.returncode:raise RuntimeError(label+': '+p.stdout[-4000:])
 print(target,label,'OK',flush=True)
run([WS/'qt-sdk/libexec/rcc',SRC/'assets/SParamView.qrc','-o',out/'qrc.cpp'],'rcc')
sources={k:SRC/'src'/(k+'.cpp') for k in ['core','window','plot','report','theme','navigation','workers','main']};sources['qrc']=out/'qrc.cpp'
def compile_one(item):
 name,path=item;obj=out/(name+'.o')
 if len(sys.argv)>2 and sys.argv[2]=='resume' and obj.exists() and obj.stat().st_size>256:return name,obj
 temporary=out/(name+'.new.o');run([compiler,*flags,'-c',path,'-o',temporary],name)
 assert temporary.stat().st_size>256,(name,'empty compiler output')
 temporary.replace(obj);return name,obj
with cf.ThreadPoolExecutor(max_workers=2) as pool:objects=dict(pool.map(compile_one,sources.items()))
libs=['-L'+str(qt/'lib'),'-lQt6Widgets','-lQt6Gui','-lQt6Concurrent','-lQt6Core']
if win:
 libs+=['-L'+str(WS/'windows-deps/dev/usr/x86_64-w64-mingw32/lib')]
 run([WS/'windows-deps/dev/usr/bin/x86_64-w64-mingw32-windres','-I',SRC/'assets',SRC/'assets/SParamView.rc','-O','coff','-o',out/'icon.o'],'windres');objects['icon']=out/'icon.o'
else:libs+=['-Wl,-rpath,'+str(qt/'lib')]
ext='.exe' if win else ''
run([compiler,*driver,'-pthread',*(['-mwindows'] if win else []),*objects.values(),*libs,'-o',out/('SParamView'+ext)],'link_app')
tests={'si_cli':SRC/'src/cli.cpp','si_tests':SRC/'tests/core_tests.cpp'}
for k in ['mapping','polarity','termination','performance','hardening','qt','navigation','open']:tests['si_'+k+'_tests']=SRC/'tests'/(k+'_tests.cpp')
for k in ['real_file','termination']:tests['si_'+k+'_probe']=SRC/'tests'/(k+'_probe.cpp')
def build_test(item):
 name,path=item;objs=[objects['core']];extra=[]
 if name=='si_qt_tests':objs += [objects[k] for k in ['workers','plot','theme','navigation']]
 if name in ['si_navigation_tests','si_open_tests']:
  objs += [objects[k] for k in ['window','workers','plot','theme','navigation','report']];extra=['-DSI_NEW_NAVIGATION']
 run([compiler,*flags,*extra,path,*objs,*libs,'-o',out/(name+ext)],name)
with cf.ThreadPoolExecutor(max_workers=2) as pool:list(pool.map(build_test,tests.items()))
shutil.copytree(SRC/'assets/fonts',out/'fonts',dirs_exist_ok=True)
(out/'complete.json').write_text(json.dumps({'target':target,'executables':['SParamView'+ext,*[k+ext for k in tests]],'compiler':compiler},indent=2))
