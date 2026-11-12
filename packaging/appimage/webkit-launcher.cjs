// Release WebKitGTK ignores WEBKIT_EXEC_PATH. Relocate its compiled-in paths
// in a private copy, without changing the read-only image or disabling sandboxing.
// Node is already included for offline mathematics; no host Python is required.
const fs = require('node:fs');
const path = require('node:path');
const {spawn} = require('node:child_process');
const appdir = path.resolve(__dirname, '../../..');
const manifest = JSON.parse(fs.readFileSync(path.join(__dirname, 'webkit-paths.json')));
// Keep this short: /tmp/sXXXXXX/b fits WebKit's /usr/bin/bwrap literal.
const temporary = fs.mkdtempSync('/tmp/s');
fs.chmodSync(temporary, 0o700);
let child;
function cleanup() { fs.rmSync(temporary, {recursive: true, force: true}); }
process.on('exit', cleanup);
try {
    const replacements = [
        [manifest.helpers, path.join(temporary, 'w'), 'usr/lib/webkit2gtk-4.1'],
        ['/usr/bin/bwrap', path.join(temporary, 'b'), 'usr/bin/bwrap'],
        ['/usr/bin/xdg-dbus-proxy', path.join(temporary, 'p'), 'usr/bin/xdg-dbus-proxy'],
    ];
    let library = fs.readFileSync(path.join(appdir, 'usr/lib', manifest.library));
    for (const [original, target, relative] of replacements) {
        fs.symlinkSync(path.join(appdir, relative), target);
        // Pad with slashes before the basename, preserving ELF offsets and also
        // relocating occurrences followed by /injected-bundle/.
        if (target.length > original.length) throw new Error('WebKit path is too short to relocate');
        const replacement = path.dirname(target) + '/'.repeat(original.length - target.length + 1) + path.basename(target);
        const before = Buffer.from(original), after = Buffer.from(replacement);
        let offset = 0, count = 0;
        while ((offset = library.indexOf(before, offset)) !== -1) {
            after.copy(library, offset); offset += before.length; count++;
        }
        if (original === manifest.helpers && !count) throw new Error('WebKit helper path was not found');
    }
    fs.writeFileSync(path.join(temporary, manifest.library), library, {mode: 0o600});
    child = spawn(path.join(__dirname, 'scalar-webkit-host.bin'), process.argv.slice(2), {
        stdio: 'inherit',
        env: {...process.env, LD_LIBRARY_PATH: temporary + ':' + path.join(appdir, 'usr/lib')},
    });
    for (const signal of ['SIGTERM', 'SIGINT', 'SIGHUP']) {
        process.on(signal, () => child.kill(signal));
    }
    child.on('error', error => { console.error(error.message); process.exitCode = 1; });
    child.on('exit', (code, signal) => { process.exitCode = code ?? (signal ? 1 : 0); });
} catch (error) {
    console.error('Scalar AppImage WebKit: ' + error.message);
    process.exitCode = 1;
}
