"""Reproduce the five-file TDR regression outside the original workspace.

Requires the original Touchstone files, a full CMake build, and:
  pip install numpy scipy scikit-rf openpyxl pillow
Usage:
  python scripts/validate_samples.py --build build --samples C:/Measurements --output validation-local
No files in --samples are modified. --output must be a new directory.
"""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--samples', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    build, samples, output = (p.resolve() for p in (args.build, args.samples, args.output))
    project = Path(__file__).resolve().parents[1]
    reference = project / 'docs/performance-validation'
    manifest = json.loads((reference / 'manifest.json').read_text(encoding='utf-8'))
    suffix = '.exe' if os.name == 'nt' else ''

    def executable(name):
        for candidate in (build / (name + suffix), build / 'Release' / (name + suffix)):
            if candidate.is_file():
                return str(candidate)
        raise FileNotFoundError(f'Build target {name} first in {build}')

    probe, app = executable('si_termination_probe'), executable('SParamView')
    for entry in manifest['files']:
        if not (samples / entry['name']).is_file():
            raise FileNotFoundError(samples / entry['name'])
    output.mkdir(parents=True, exist_ok=False)
    for name in ('manifest.json', 'crosscheck.py', 'verify_gui.py'):
        shutil.copy2(reference / name, output / name)
    shutil.copytree(reference / 'mappings', output / 'mappings')
    (output / 'projects').mkdir()
    environment = dict(os.environ, QT_QPA_PLATFORM='offscreen', SI_VALIDATION_UI='1', OPENBLAS_NUM_THREADS='1')
    runs = []
    with (output / 'execution.log').open('w', encoding='utf-8') as log:
        def run(command):
            subprocess.run(command, env=environment, stdout=log, stderr=subprocess.STDOUT, check=True)
            log.flush()

        for entry in manifest['files']:
            key = entry['key']
            for reverse in (False, True):
                case = key + ('_Reverse' if reverse else '_Forward')
                run([probe, str(samples / entry['name']), str(output / 'mappings' / (key + '.csv')),
                     str(output / 'cache'), str(output / 'traces' / case), str(int(reverse))])
                for rho in (False, True):
                    name = case + ('_rho' if rho else '_ohm')
                    original = project / 'docs/termination-validation/projects' / (name + '.siproject')
                    settings = json.loads(original.read_text(encoding='utf-8'))
                    settings['revisions'][0]['source'] = str(samples / entry['name'])
                    current = output / 'projects' / original.name
                    current.write_text(json.dumps(settings, indent=2), encoding='utf-8')
                    run([app, '--validate-project', str(current), str(output / 'gui' / name)])
                    runs.append({'project': name})
                    print(name, 'PASS', flush=True)
        (output / 'gui_runs.json').write_text(json.dumps(runs, indent=2), encoding='utf-8')
        run([sys.executable, str(output / 'crosscheck.py'), str(samples)])
        run([sys.executable, str(output / 'verify_gui.py')])
    print('PASS: inspect crosscheck.json, workbook_check.json, execution.log in', output)


if __name__ == '__main__':
    main()
