#!/usr/bin/env python3
"""Require a real WebKit page load, including JavaScript, before publication."""
import json
import os
from pathlib import Path
import selectors
import signal
import subprocess
import sys
import tempfile
import time

appdir = Path(sys.argv[1]).resolve()
with tempfile.TemporaryDirectory(prefix='scalar-webkit-test-') as work:
    work = Path(work)
    environment = dict(os.environ, HOME=str(work), XDG_CONFIG_HOME=str(work / 'config'),
                       XDG_DATA_HOME=str(work / 'data'), XDG_CACHE_HOME=str(work / 'cache'))
    # The same hooks and launcher used by the real application, not a system host.
    command = ['bash', '-c', '''
set -e
export APPDIR="$1"
source "$APPDIR/usr/libexec/scalar/runtime-env.sh"
exec "$APPDIR/usr/libexec/scalar/scalar-webkit-host" "$2"
''', 'scalar-webkit-test', str(appdir), str(work / 'profile')]
    with (work / 'stderr').open('w+') as errors:
        process = subprocess.Popen(command, env=environment, stdin=subprocess.PIPE,
                                   stdout=subprocess.PIPE, stderr=errors, start_new_session=True)
        selector = selectors.DefaultSelector()
        selector.register(process.stdout, selectors.EVENT_READ)
        success = False
        buffer = b''
        try:
            deadline = time.monotonic() + 45
            while time.monotonic() < deadline and process.poll() is None:
                for key, _ in selector.select(timeout=1):
                    data = os.read(key.fileobj.fileno(), 65536)
                    if not data:
                        break
                    buffer += data
                    while b'\n' in buffer:
                        line, buffer = buffer.split(b'\n', 1)
                        event = json.loads(line)
                        if event.get('event') == 'ready':
                            page = 'data:text/html,<script>document.title="ScalarAppImageOK"</script>'
                            process.stdin.write((json.dumps({'op': 'navigate', 'url': page}) + '\n').encode())
                            process.stdin.flush()
                        if event.get('title') == 'ScalarAppImageOK':
                            success = True
                if success:
                    break
        finally:
            process.stdin.close()
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                os.killpg(process.pid, signal.SIGTERM)
                try:
                    process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    os.killpg(process.pid, signal.SIGKILL)
                    process.wait()
            selector.close()
        if not success:
            errors.seek(0)
            raise SystemExit('Falha no navegador integrado do AppImage:\n' + errors.read())
print('WebKit empacotado: carregamento e JavaScript OK')
