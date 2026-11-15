#!/usr/bin/env python3
"""Run *official* IEEE P802.3ck COM 3.70 against a reproducible synthetic S4P.

Runs in an ephemeral CI directory. Neither reference source nor Excel config is
committed/re-distributed. This does not by itself certify the *published* 802.3ck
normative COM profile or agreement with an independent oracle.
"""
from __future__ import annotations
import hashlib, io, json, math, os, pathlib, re, shutil, subprocess, sys, tempfile, zipfile
from urllib.request import Request,urlopen

ARCHIVE_URL="https://www.ieee802.org/3/ck/public/tools/tools/mellitz_3ck_adhoc_01_032322_COM3p70.zip"
ARCHIVE_SHA256="0accfa982bbb85d3ee4b368c889d1e1126a91662dce3e7eac18ab75591351bbb"
CODE_NAME="com_ieee8023_93a_370.m"
CONFIG_NAME="config_sheets_3p1/config_com_ieee8023_93a=3ck_d3p1_120F_C2C_11_30_21.xlsx"

def fixture(path: pathlib.Path, loss_at_nyquist_db: float) -> str:
    """Passive, reciprocal 4-port with loss and time delay; source data original."""
    nyquist=26.5625
    with path.open("w",encoding="ascii") as fp:
        fp.write("! 012_02 original analytic reciprocal 4-port THRU, not IEEE supplied\n")
        fp.write("# GHZ S RI R 50\n")
        # 0.05 GHz start and 0.01 GHz step, nominal receiver Nyquist 26.5625 GHz.
        # Port 1/2 = input pair, 3/4 = output pair. Touchstone is column major.
        for j in range(6000):
            fghz=0.05 + 0.01*j
            loss_db=loss_at_nyquist_db * math.sqrt(fghz/nyquist)
            amp=10**(-loss_db/20)
            phase=-2*math.pi*fghz*1e9*0.30e-9
            hr=amp*math.cos(phase)
            hi=amp*math.sin(phase)
            S=[[0j for _ in range(4)] for _ in range(4)]
            for i,k in ((0,2),(1,3)):
                S[i][k]=complex(hr,hi)
                S[k][i]=complex(hr,hi)
            vals=[f"{fghz:.4f}"]
            for col in range(4):
                for row in range(4):
                    z=S[row][col]
                    vals.extend((f"{z.real:.10e}", f"{z.imag:.10e}"))
            fp.write(" ".join(vals)+"\n")
    return hashlib.sha256(path.read_bytes()).hexdigest()

def main():
    req=Request(ARCHIVE_URL, headers={"User-Agent":"SParamView-012_02-RefRun/1.0"})
    with urlopen(req,timeout=50) as response:
        payload=response.read(60_000_001)
    if len(payload)>60_000_000 or hashlib.sha256(payload).hexdigest()!=ARCHIVE_SHA256:
        raise RuntimeError("Reference bundle archive SHA256 mismatch")
    with zipfile.ZipFile(io.BytesIO(payload)) as z, tempfile.TemporaryDirectory(prefix="sparam-com-370-") as td:
        root=pathlib.Path(td)
        code=z.read(CODE_NAME)
        config=z.read(CONFIG_NAME)
        source_path=root/"com_ieee8023_93a.m" # function declared com_ieee8023_93a, not _370
        original_text=code.decode("utf-8","replace")
        for i,line in enumerate(original_text.splitlines(),1):
            if "verLessThan" in line:
                print(f"REFERENCE_MATLAB_VERSION_CHECK_ORIGINAL_LINE {i}: {line[:160]}",flush=True)
        # Adapter ONLY for MATLAB package-version queries; original reference
        # source and its SHA remain independently pinned. Emulates an R2022b
        # installation for version-branch selection; not numerical qualification.
        matlab_compat_version=(9,13,0)
        adapted=[]
        def adapt_matlab_version(match):
            target=tuple(int(n) for n in match.group(1).split("."))
            target=target+(0,)*(3-len(target))
            value="true" if matlab_compat_version<target else "false"
            adapted.append({"original":match.group(0),"replacement":value})
            return value
        rx=r"""verLessThan\s*\(\s*['"]matlab['"]\s*,\s*['"]([0-9.]+)['"]\s*\)"""
        adapted_text=re.sub(rx,adapt_matlab_version,original_text,flags=re.I)
        print("REFERENCE_OCTAVE_MATLAB_VERSION_ADAPTER",
              json.dumps(adapted,ensure_ascii=False),flush=True)
        if not adapted:
            raise RuntimeError("Cannot validate MATLAB version call-site; refuse silent patch")
        source_path.write_text(adapted_text,encoding="utf-8")
        print("REFERENCE_ADAPTED_SOURCE_SHA256",
              hashlib.sha256(source_path.read_bytes()).hexdigest(),flush=True)
        config_path=root/"reference_120f_c2c.xlsx"
        config_path.write_bytes(config)
        s4p_path=root/"synthetic_c2c_thru.s4p"
        input_hash=fixture(s4p_path,loss_at_nyquist_db=10.0)
        report_path=root/"result.txt"
        runner=root/"run_reference.m"
        script=f"""
try
  pkg load io;
  set(0, 'defaultfigurevisible', 'off');
  cd('{str(root).replace("'", "''")}');
  fprintf('IEEECOM_START\\n');
  tic;
  result = com_ieee8023_93a('{config_path.name}', 0, 0, '{s4p_path.name}');
  fprintf('IEEECOM_ELAPSED_SEC %.3f\\n', toc);
  disp('IEEECOM_RESULT_TYPE'); disp(class(result));
  disp('IEEECOM_RESULT_CONTENT'); disp(result);
  fprintf('IEEECOM_RUN_FINISHED\\n');
catch err
  fprintf(2,'IEEECOM_ERROR %s\\n',err.message);
  for k=1:min(numel(err.stack),12)
    fprintf(2,'IEEECOM_STACK %s:%d\\n',err.stack(k).name,err.stack(k).line);
  end
  exit(2);
end
exit(0);
"""
        runner.write_text(script,encoding="utf-8")
        print(json.dumps({"run":"IEEE_8023CK_COM_370_REAL_REFERENCE_ATTEMPT",
          "reference_archive_sha256":ARCHIVE_SHA256,
          "reference_code_sha256":hashlib.sha256(code).hexdigest(),
          "reference_config_sha256":hashlib.sha256(config).hexdigest(),
          "synthetic_s4p_sha256":input_hash,
          "synthetic_s4p_rows":6000,"f_start_ghz":0.05,
          "f_step_ghz":0.01,"loss_at_nyquist_db":10.0},indent=2),flush=True)
        exe=shutil.which("octave-cli") or shutil.which("octave")
        if not exe:
            print("IEEECOM_ERROR: Octave not installed; reference not executed",file=sys.stderr)
            return 2
        proc=subprocess.run([exe,"--no-gui","--no-window-system","--quiet",str(runner)],cwd=root,
            timeout=480,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,errors="replace")
        report_path.write_text(proc.stdout,encoding="utf-8")
        print("BEGIN_OFFICIAL_REFERENCE_EXECUTION_OUTPUT\n"+proc.stdout[-16000:]+
              "\nEND_OFFICIAL_REFERENCE_EXECUTION_OUTPUT",flush=True)
        if proc.returncode!=0 or "IEEECOM_RUN_FINISHED" not in proc.stdout:
            print(f"OFFICIAL_COM_NUMERIC_REFERENCE_NOT_REPRODUCED exit={proc.returncode}",file=sys.stderr)
            return 2
        print("OFFICIAL_COM_EXECUTION_FINISHED (not yet qualified against independent COM goldens)")
        return 0

if __name__=="__main__":
    try:
        sys.exit(main())
    except Exception as error:
        print(f"OFFICIAL_COM_NUMERIC_REFERENCE_NOT_REPRODUCED: {type(error).__name__}: {error}",file=sys.stderr)
        sys.exit(2)
