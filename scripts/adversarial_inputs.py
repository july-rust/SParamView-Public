"""Deterministic process-isolated malformed Touchstone tests (seed 20260914).
Invalid mutations may remain valid: those cases only assert bounded safe exit.
Run: python scripts/adversarial_inputs.py BUILD/si_cli OUTPUT
"""
from pathlib import Path
import hashlib, json, os, random, subprocess, sys, time

cli=Path(sys.argv[1]).resolve(); root=Path(sys.argv[2]).resolve();root.mkdir(parents=True,exist_ok=True)
rng=random.Random(20260914)
base=b'# Hz S RI R 50\n1 '+b'.1 0 '*16+b'\n2 '+b'.2 0 '*16+b'\n'
cases=[]
for label,body,ports in [
    ('nan',base.replace(b'.1',b'nan',1),4),('inf',base.replace(b'.1',b'inf',1),4),
    ('negative_frequency',base.replace(b'\n1 ',b'\n-1 ',1),4),
    ('duplicate_frequency',base.replace(b'\n2 ',b'\n1 ',1),4),
    ('negative_reference',base.replace(b'R 50',b'R -50'),4),
    ('huge_ports',b'# Hz S RI R 50\n1 0 0\n',1000000000),
    ('overlong_number',b'# Hz S RI R 50\n'+b'9'*5000+b' 0 0\n',1),
    ('overlong_keyword',b'['+b'X'*200+b']\n',1),
    ('invalid_format',base.replace(b'RI',b'XX'),4),
    ('truncated_pair',base[:-5],4),
]: cases.append((label,body,ports,True))
for i in range(400):
    b=bytearray(base)
    mode=i%4
    if mode==0: b=b[:rng.randrange(len(b))]
    elif mode==1:
        for _ in range(rng.randint(1,8)): b[rng.randrange(len(b))]=rng.randrange(256)
    elif mode==2:
        start=rng.randrange(len(b));b[start:start]=rng.choice([b'\x00',b'[End]',b'NaN',b'1e999',b'!',b'\r',b'\t'])
    else:
        start=rng.randrange(len(b));del b[start:start+rng.randrange(1,20)]
    cases.append((f'mutation_{i:03}',bytes(b),4,False))
results=[];begin=time.perf_counter()
for label,body,ports,must_reject in cases:
    p=root/f'{label}.s{ports}p';p.write_bytes(body)
    try:
        r=subprocess.run([str(cli),'inspect',str(p),str(root/'cache')],capture_output=True,timeout=5)
        passed=r.returncode in ([1] if must_reject else [0,1])
        message=r.stderr.decode('utf-8',errors='replace')[:1500]
        code=r.returncode
    except subprocess.TimeoutExpired:
        passed=False;message='TIMEOUT';code=None
    assert hashlib.sha256(p.read_bytes()).digest()==hashlib.sha256(body).digest()
    results.append(dict(case=label,returncode=code,passed=passed,must_reject=must_reject,stderr=message))
    if not passed: print('FAIL',label,code,message,flush=True)
report=dict(seed=20260914,cases=len(results),passed=sum(r['passed'] for r in results),seconds=time.perf_counter()-begin,source_unchanged=True,results=results)
(root/'adversarial.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
print({k:v for k,v in report.items() if k!='results'})
raise SystemExit(0 if report['passed']==report['cases'] else 1)
