"""Visible Calc/UNO Basic round-trip. Interactive import/save dialogs are separate UNVERIFIED scope."""
import argparse, json, os, pathlib, platform, shutil, subprocess, time
from xml.sax.saxutils import escape
p=argparse.ArgumentParser();p.add_argument('probe',type=pathlib.Path);p.add_argument('output',type=pathlib.Path);p.add_argument('--office');a=p.parse_args()
a.probe=a.probe.resolve();a.output=a.output.resolve();a.output.mkdir(parents=True,exist_ok=True)
record={'os':platform.platform(),'source_commit':os.getenv('GITHUB_SHA','local-working-tree'),
        'interactive_import_save_dialogs':'UNVERIFIED',
        'excel':'UNVERIFIED — Excel execution environment unavailable',
        'scope':'Visible Calc document via supported UNO Basic API; dialogs bypassed by explicit FilterOptions'}
paths=[a.office,shutil.which('libreoffice'),shutil.which('soffice'),
       r'C:\Program Files\LibreOffice\program\soffice.exe','/Applications/LibreOffice.app/Contents/MacOS/soffice']
office=next((str(x) for x in paths if x and pathlib.Path(x).exists()),None)
if not office:
    record.update(status='UNVERIFIED',reason='LibreOffice executable unavailable')
    (a.output/'results.json').write_text(json.dumps(record,indent=2),encoding='utf-8');raise SystemExit(0)
record['office']=office
try: record['version']=subprocess.check_output([office,'--version'],timeout=30,text=True).strip()
except Exception as e: record['version_error']=str(e)
profile=a.output/'profile'
# First-run initialization installs Standard/Module1; do it BEFORE our macro.
subprocess.run([office,'-env:UserInstallation='+profile.as_uri(),'--headless','--terminate_after_init'],timeout=45,check=True)
standard=profile/'user/basic/Standard';standard.mkdir(parents=True,exist_ok=True)
(profile/'user/basic/script.xlc').write_text('''<?xml version="1.0"?><library:libraries xmlns:library="http://openoffice.org/2000/library" xmlns:xlink="http://www.w3.org/1999/xlink"><library:library library:name="Standard" xlink:href="'''+escape((standard/'script.xlb').as_uri())+'''" xlink:type="simple" library:link="false"/></library:libraries>''')
(standard/'script.xlb').write_text('''<?xml version="1.0"?><library:library xmlns:library="http://openoffice.org/2000/library" library:name="Standard" library:readonly="false" library:passwordprotected="false"><library:element library:name="Module1"/></library:library>''')
(standard/'dialog.xlb').write_text('''<?xml version="1.0"?><library:library xmlns:library="http://openoffice.org/2000/library" library:name="Standard" library:readonly="false" library:passwordprotected="false"/>''')
source=a.output/'source.csv';saved=a.output/'saved.csv';audit=a.output/'cells.tsv';ready=a.output/'visible.ready';go=a.output/'continue';done=a.output/'done';control=a.output/'control.csv'
subprocess.run([str(a.probe),'generate',str(source)],check=True)
control.write_text('value\n=1+2\n',encoding='utf-8')
# Explicitly enabled formula import is independently checked using a benign control.
options='44,34,76,1,,0,false,true,true,false,false,0,true,false,true'
record['filter_options']=options
basic='''Sub Main
On Error GoTo Failed
Dim props(2) As New com.sun.star.beans.PropertyValue
props(0).Name = "FilterName"
props(0).Value = "Text - txt - csv (StarCalc)"
props(1).Name = "FilterOptions"
props(1).Value = "OPTIONS"
props(2).Name = "Hidden"
props(2).Value = False
Dim doc As Object, sh As Object, cell As Object
Dim f As Integer, r As Long, c As Variant
doc = StarDesktop.loadComponentFromURL("SOURCE", "_blank", 0, props())
sh = doc.Sheets(0)
f = FreeFile
Open "AUDIT" For Output As #f
For r = 1 To 32
For Each c In Array(0,5,6,7)
cell = sh.getCellByPosition(c,r)
Print #f, r & Chr(9) & c & Chr(9) & cell.Type & Chr(9) & cell.Formula
Next c
Next r
Close #f
f = FreeFile
Open "READY" For Output As #f
Print #f, "Visible Calc document loaded"
Close #f
For r = 1 To 300
If FileExists("GO") Then Exit For
Wait 100
Next r
doc.storeAsURL("SAVED",props())
doc.close(True)
doc = StarDesktop.loadComponentFromURL("CONTROL", "_blank", 0, props())
cell = doc.Sheets(0).getCellByPosition(0,1)
f = FreeFile
Open "DONE" For Output As #f
Print #f, "control_type=" & cell.Type
Print #f, "control_value=" & cell.Value
Close #f
doc.close(True)
StarDesktop.terminate()
Exit Sub
Failed:
f = FreeFile
Open "DONE" For Output As #f
Print #f, "ERROR=" & Err & ":" & Error$
Close #f
StarDesktop.terminate()
End Sub
'''
for key,value in [('OPTIONS',options),('SOURCE',source.as_uri()),('SAVED',saved.as_uri()),('CONTROL',control.as_uri()),('AUDIT',str(audit)),('READY',str(ready)),('GO',str(go)),('DONE',str(done))]: basic=basic.replace(key,value.replace('"','""'))
(standard/'Module1.xba').write_text('<?xml version="1.0"?><script:module xmlns:script="http://openoffice.org/2000/script" script:name="Module1" script:language="StarBasic">'+escape(basic)+'</script:module>',encoding='utf-8')
command=[office,'-env:UserInstallation='+profile.as_uri(),'--norestore','--nodefault','macro:///Standard.Module1.Main']
record['command']=command
with (a.output/'office.log').open('wb') as log:
    child=subprocess.Popen(command,stdout=log,stderr=subprocess.STDOUT)
    deadline=time.monotonic()+60
    while not ready.exists() and not done.exists() and time.monotonic()<deadline:time.sleep(.2)
    if ready.exists():
        # OS screenshot documents a visible GUI; absence is never replaced by headless output.
        shot=a.output/'calc.png'
        try:
            if platform.system()=='Darwin': subprocess.run(['screencapture','-x',str(shot)],check=True,timeout=15)
            elif platform.system()=='Windows':
                ps="Add-Type -AssemblyName System.Windows.Forms; Add-Type -AssemblyName System.Drawing; $r=[Windows.Forms.SystemInformation]::VirtualScreen; $b=New-Object Drawing.Bitmap $r.Width,$r.Height; $g=[Drawing.Graphics]::FromImage($b); $g.CopyFromScreen($r.Left,$r.Top,0,0,$b.Size); $b.Save('"+str(shot).replace("'","''")+"'); $g.Dispose(); $b.Dispose()"
                subprocess.run(['powershell','-NoProfile','-Command',ps],check=True,timeout=20)
            record['screenshot']=shot.exists()
        except Exception as e:record['screenshot_error']=str(e)
        go.write_text('continue')
    deadline=time.monotonic()+45
    while not done.exists() and time.monotonic()<deadline:time.sleep(.2)
    try:child.wait(timeout=10)
    except subprocess.TimeoutExpired:child.kill();child.wait()
record['exit_code']=child.returncode
if not done.exists() or not saved.exists() or not audit.exists():
    record.update(status='UNVERIFIED',reason='Visible Basic automation did not complete; see office.log')
else:
    comparison=subprocess.run([str(a.probe),str(saved)],capture_output=True,text=True)
    (a.output/'comparison.log').write_text(comparison.stdout+comparison.stderr,encoding='utf-8')
    rows=[line.split('\t',3) for line in audit.read_text(errors='replace').splitlines()]
    types=[int(r[2]) for r in rows if len(r)>=3]
    control_result=done.read_text(errors='replace')
    record.update(text_cells=len(types),string_cells=types.count(2),formula_cells=types.count(3),control=control_result,comparison_exit=comparison.returncode)
    ok=comparison.returncode==0 and len(types)==128 and all(t==2 for t in types) and 'control_type=3' in control_result and 'control_value=3' in control_result
    record.update(status='PASS' if ok else 'FAIL')
(a.output/'results.json').write_text(json.dumps(record,indent=2,ensure_ascii=False),encoding='utf-8')
print(json.dumps(record,indent=2,ensure_ascii=False))
raise SystemExit(1 if record['status']=='FAIL' else 0)
