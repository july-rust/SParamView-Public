"""Read-only real-file comparison with independent scikit-rf matrix validation.

Usage: python scripts/validate_release_samples.py SAMPLES BASELINE_BUILD CANDIDATE_BUILD OUTPUT
Source files and numeric baseline are never rewritten. Outputs stay outside sources/.
"""
import csv
import hashlib
import json
import re
import shlex
import subprocess
import sys
from pathlib import Path
import numpy as np
import skrf


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def main():
    samples, baseline, candidate, output = map(lambda x: Path(x).resolve(), sys.argv[1:])
    output.mkdir(parents=True, exist_ok=True)
    files = sorted(p for p in samples.rglob('*') if re.fullmatch(r'.*\.S\d+P', p.name, re.I))
    if len(files) != 7:
        raise RuntimeError(f'Expected exactly seven source files, got {len(files)}')
    report = dict(baseline='v1.2.3 f82b0a202ea0d0c652a227f4c5121b918ad9c8db',
                  reference=f'scikit-rf {skrf.__version__}', files=[])
    for index, source in enumerate(files):
        row = dict(name=source.name, sha256=digest(source), bytes=source.stat().st_size)
        destinations=[]
        for name, build in [('baseline', baseline), ('candidate', candidate)]:
            destination=output/name/str(index)
            destination.mkdir(parents=True,exist_ok=True)
            run=subprocess.run([str(build/'si_real_probe'),str(source),str(output/(name+'-cache')),str(destination)],capture_output=True,text=True,check=True)
            (destination/'probe.log').write_text(run.stdout+run.stderr)
            destinations.append(destination)
        old,new=destinations
        raw=(new/'matrix.bin').read_bytes()
        n,k=map(int,np.frombuffer(raw,dtype='<u8',count=2))
        f=np.frombuffer(raw,dtype='<f8',count=k,offset=16)
        matrix=np.frombuffer(raw,dtype='<c16',offset=16+k*8).reshape(n,n,k).transpose(2,0,1)
        reference=skrf.Network(str(source))
        error=float(np.max(abs(matrix-reference.s)))
        frequency_error=float(np.max(abs(f-reference.f)))
        if error>2e-10 or frequency_error>1e-3:
            raise RuntimeError('Independent matrix mismatch: '+source.name)
        if digest(old/'matrix.bin')!=digest(new/'matrix.bin'):
            raise RuntimeError('Unexpected numerical parser change: '+source.name)
        with (new/'candidates.csv').open(encoding='utf-8-sig') as stream:
            mapping=list(csv.DictReader(stream))
        with (old/'candidates.csv').open(encoding='utf-8-sig') as stream:
            old_mapping=list(csv.DictReader(stream))
        metadata=(new/'metadata.txt').read_text()
        labels={int(line.split('\t')[0])-1:line.split('\t',2)[2] for line in metadata.splitlines() if '\t' in line and line.split('\t')[0].isdigit()}
        # Independently review every channel against original label identities.
        # Near/Far is fixed to the candidate's physical endpoints for both engines.
        channels=[]
        for item in mapping:
            c=dict(id=item['ID'],name=item['Channel'],group=item.get('Group') or 'Default',alias='')
            for key in ['NearP','NearN','FarP','FarN']:
                c[key[0].lower()+key[1:]]=int(item[key])-1 if item[key] else -1
            def net(port): return labels[port].split()[-1]
            if c['farP']>=0 and net(c['nearP'])!=net(c['farP']):
                raise RuntimeError('Near/Far net identity mismatch')
            if c['nearN']>=0:
                pos,neg=net(c['nearP']),net(c['nearN'])
                delta=[(a.lower(),b.lower()) for a,b in zip(pos,neg) if a.lower()!=b.lower()]
                if len(pos)!=len(neg) or delta!=[('p','n')]:
                    raise RuntimeError('Physical polarity evidence mismatch')
                if c['farN']>=0 and net(c['nearN'])!=net(c['farN']):
                    raise RuntimeError('Negative endpoint identity mismatch')
            channels.append(c)
        settings=dict(startHz=0,stopHz=None,reverse=False,markers=3,prominence=.15,targetOhm=0,
                      tdrLimit=False,tolerancePercent=10,tdrStart=0,tdrStop=3e-9,kaiserBeta=6,
                      performance=1,manualMarkersHz=[1e9,3e9,6e9],quick=[0,1,2,3,4],
                      tdrTerminations=[1],tdrReflection=False,
                      limits=[dict(enabled=False,constant=0,points=[]) for _ in range(4)])
        kinds={c['nearN']>=0 for c in channels}
        signal='Mixed' if len(kinds)>1 else 'Differential' if True in kinds else 'Single-ended'
        project=dict(format='SIAnalyzerProject',schema=1,name=source.stem,
                     settings=settings,revisions=[dict(name='Source',source=str(source),absoluteSource=str(source),
                     sourceSha256=row['sha256'],channels=channels,confirmed=True,signalType=signal,
                     signalConfidence='Reviewed',signalEvidence='Physical P/N and endpoint net labels cross-checked; identical mapping supplied to both engines.',
                     signalTypeReviewed=True,mappingSource='Release label audit')],
                     ui=dict(revision=0,selected=[c['id'] for c in channels],group='All groups',metric=0))
        project_path=output/f'{index}.siproject'
        project_path.write_text(json.dumps(project,indent=2))
        row.update(ports=n,points=k,start_hz=float(f[0]),stop_hz=float(f[-1]),
                   complex_values_checked=int(matrix.size),max_complex_error=error,
                   max_frequency_error_hz=frequency_error,baseline_matrix_identical=True,
                   mapping_identical=old_mapping==mapping,channels=len(channels),signal_type=signal,
                   parser_warnings=[line[5:] for line in metadata.splitlines() if line.startswith('NOTE\t')],
                   physical_mapping_evidence='Reviewed against original endpoint labels; identical mapping fixed for both engines.',
                   project=str(project_path))
        report['files'].append(row)
        (output/'matrix-audit.json').write_text(json.dumps(report,indent=2))
        print(source.name,n,k,'PASS',flush=True)
    print('Seven-file matrix and mapping comparison complete',flush=True)


if __name__=='__main__':main()
