"""Compare instrumented baseline/candidate GUI exports using the seven-file audit.

Usage: python scripts/validate_release_exports.py AUDIT_DIR BASELINE_APP CANDIDATE_APP
Both apps provide --validate-project; only export instrumentation is added to baseline.
Requires openpyxl. Raw customer files are read-only and are never copied to the report.
"""
import hashlib
import json
import subprocess
import sys
from pathlib import Path
import openpyxl


def sha(path):
    with Path(path).open('rb') as stream:return hashlib.file_digest(stream,'sha256').hexdigest()


def main():
    root=Path(sys.argv[1]).resolve()
    apps=[Path(a).resolve() for a in sys.argv[2:]]
    audit=json.loads((root/'matrix-audit.json').read_text())
    rows=[]
    for index,source in enumerate(audit['files']):
        project=root/f'{index}.siproject'
        dirs=[]
        for name,app in zip(['baseline-gui','candidate-gui'],apps):
            out=root/name/str(index);out.mkdir(parents=True,exist_ok=True)
            run=subprocess.run([str(app),'--validate-project',str(project),str(out)],capture_output=True,text=True,timeout=600)
            (out/'process.log').write_text(run.stdout+run.stderr)
            if run.returncode:raise RuntimeError(f'{name} {source["name"]} failed: {run.stderr[-1000:]}')
            dirs.append(out)
        before,after=dirs
        old=json.loads((before/'Validation.json').read_text())
        new=json.loads((after/'Validation.json').read_text())
        if old['analysis']!=new['analysis']:raise RuntimeError('Full analysis results differ')
        csvs=sorted(after.glob('*.csv'))
        if not csvs or any(sha(p)!=sha(before/p.name) for p in csvs):raise RuntimeError('CSV trace mismatch')
        workbooks=[openpyxl.load_workbook(p/'Analysis.xlsx',read_only=True,data_only=True) for p in dirs]
        sheets=['Summary','Return_Loss','Insertion_Loss','NEXT','FEXT','TDR']
        counts={}
        for sheet in sheets:
            values=[list(w[sheet].values) for w in workbooks]
            if values[0]!=values[1]:raise RuntimeError('Excel numerical sheet differs: '+sheet)
            counts[sheet]=len(values[1])-1
        config=dict(workbooks[1]['Configuration'].values)
        required=['Signal Type','Confidence','Evidence','Mapping Source','Mapping confirmed',
                  'Source SHA-256','Source SHA-256 verification','Analysis completed UTC','Report generated UTC','Settings JSON']
        for key in required:
            if not config.get(key):raise RuntimeError('Missing Excel provenance '+key)
        if config['Source SHA-256']!=source['sha256']:raise RuntimeError('Excel source identity mismatch')
        if config['Analysis completed UTC']>config['Report generated UTC']:raise RuntimeError('Invalid provenance timestamps')
        for workbook in workbooks:workbook.close()
        for out in dirs:
            saved=json.loads((out/'roundtrip.siproject').read_text())
            original=json.loads(project.read_text())
            if saved['revisions'][0]['channels']!=original['revisions'][0]['channels']:raise RuntimeError('Project mapping mismatch')
        original=json.loads(project.read_text())['revisions'][0]['absoluteSource']
        if sha(original)!=source['sha256']:raise RuntimeError('Source modified during validation')
        rows.append(dict(name=source['name'],sha256=source['sha256'],analysis_results=len(new['analysis']),
                         exact_analysis_match=True,raw_csv_count=len(csvs),raw_csv_byte_identical=True,
                         excel_rows=counts,excel_numerical_sheets_identical=True,excel_provenance=True,
                         project_roundtrip=bool(new['project_roundtrip']),source_unchanged=True))
        (root/'export-audit.json').write_text(json.dumps(dict(files=rows),indent=2))
        print(source['name'],len(new['analysis']),'results,',len(csvs),'CSV: PASS',flush=True)


if __name__=='__main__':main()
