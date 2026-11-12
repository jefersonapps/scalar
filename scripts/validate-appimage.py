#!/usr/bin/env python3
"""Audit every packaged ELF, not just the main executable."""
import json
import os
from pathlib import Path
import re
import subprocess
import sys

appdir = Path(sys.argv[1]).resolve()
helpers = sys.argv[2]
container = sys.argv[3] == '--container'
environment = dict(os.environ, LD_LIBRARY_PATH=str(appdir / 'usr/lib'))
maximum = (0, 0)
external = set()
for item in appdir.rglob('*'):
    if not item.is_file() or item.is_symlink():
        continue
    with item.open('rb') as stream:
        if stream.read(4) != b'\x7fELF':
            continue
    result = subprocess.run(['ldd', str(item)], env=environment, text=True, capture_output=True)
    if 'not found' in result.stdout + result.stderr:
        raise SystemExit(f'Dependência ausente em {item}:\n{result.stdout}{result.stderr}')
    for name, resolved in re.findall(r'^\s*(\S+) => (/\S+)', result.stdout, re.MULTILINE):
        if not Path(resolved).resolve().is_relative_to(appdir):
            external.add(name)
            if name.startswith(('libQt6', 'libwebkit', 'libjavascriptcore', 'libgtk-3', 'libsoup', 'libgst')):
                raise SystemExit(f'{item.name} depende de {name} fora do AppImage: {resolved}')
    versions = subprocess.check_output(['readelf', '--version-info', str(item)], text=True)
    for version in re.findall(r'Name: GLIBC_(\d+\.\d+)', versions):
        maximum = max(maximum, tuple(map(int, version.split('.'))))
if container and maximum > (2, 36):
    raise SystemExit(f'O pacote exige glibc {maximum}, acima da base 2.36')
library = appdir / 'usr/lib/libwebkit2gtk-4.1.so.0'
if helpers.encode() not in library.read_bytes():
    raise SystemExit('WebKit mudou o caminho de seus processos; revise o launcher antes de distribuir.')
(appdir / 'usr/libexec/scalar/webkit-paths.json').write_text(json.dumps({
    'helpers': helpers, 'library': library.name,
}) + '\n')
subprocess.run(['desktop-file-validate', str(appdir / 'usr/share/applications/scalar.desktop')], check=True)
node = appdir / 'usr/share/scalar/math/node'
subprocess.run([str(node), '--check', str(appdir / 'usr/libexec/scalar/webkit-launcher.cjs')], env=environment, check=True)
math = subprocess.run([str(node), str(appdir / 'usr/share/scalar/math/tex-svg.cjs')],
                      input='[{"latex":"x^2","display":false}]', text=True,
                      capture_output=True, env=environment, check=True, timeout=30)
if '<svg' not in json.loads(math.stdout)[0]['svg']:
    raise SystemExit('Falha no teste de matemática offline')
report = f'glibc mínima detectada: {".".join(map(str, maximum))}\n'
report += 'Bibliotecas fornecidas pelo sistema:\n' + '\n'.join(sorted(external)) + '\n'
(appdir / 'usr/share/scalar/appimage-build.txt').write_text(report)
print(report, end='')
