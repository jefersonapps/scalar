#!/usr/bin/env python3
"""Installer/build-time bootstrap; end users receive a complete offline runtime."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import shutil
import subprocess
import tarfile
import tempfile
import urllib.request
import zipfile

NODE_VERSION = "22.23.3"


def download(url, destination):
    with urllib.request.urlopen(url, timeout=45) as response, destination.open("wb") as output:
        shutil.copyfileobj(response, output)


def prepare(destination):
    destination.mkdir(parents=True, exist_ok=True)
    system = {"Linux": "linux", "Windows": "win"}.get(platform.system())
    arch = {"x86_64": "x64", "AMD64": "x64", "aarch64": "arm64", "ARM64": "arm64"}.get(platform.machine())
    if not system or not arch:
        raise RuntimeError("Unsupported platform for the Scalar math runtime")
    binary = destination / ("node.exe" if system == "win" else "node")
    package = destination / "node_modules/mathjax-full/package.json"
    complete = destination / ".scalar-runtime.json"
    expected = {"node": NODE_VERSION, "mathjax-full": "3.2.2"}
    if binary.is_file() and package.is_file() and complete.is_file() and json.loads(complete.read_text()) == expected:
        return
    archive_name = f"node-v{NODE_VERSION}-{system}-{arch}." + ("zip" if system == "win" else "tar.xz")
    base = f"https://nodejs.org/dist/v{NODE_VERSION}/"
    with tempfile.TemporaryDirectory(prefix="scalar-math-") as work:
        work = Path(work)
        archive = work / archive_name
        download(base + archive_name, archive)
        download(base + "SHASUMS256.txt", work / "checksums")
        hashes = {name: value for value, name in (line.split() for line in (work / "checksums").read_text().splitlines())}
        if hashlib.sha256(archive.read_bytes()).hexdigest() != hashes.get(archive_name):
            raise RuntimeError("Node runtime checksum mismatch")
        if system == "win":
            with zipfile.ZipFile(archive) as source:
                source.extractall(work)
        else:
            with tarfile.open(archive) as source:
                source.extractall(work, filter="data")
        root = work / f"node-v{NODE_VERSION}-{system}-{arch}"
        node = root / ("node.exe" if system == "win" else "bin/node")
        npm = root / ("node_modules/npm/bin/npm-cli.js" if system == "win" else "lib/node_modules/npm/bin/npm-cli.js")
        environment = dict(os.environ)
        environment["PATH"] = str(node.parent) + os.pathsep + environment.get("PATH", "")
        subprocess.run([str(node), str(npm), "install", "--prefix", str(destination), "--omit=dev", "--ignore-scripts", "--no-audit", "--no-fund"], check=True, env=environment, timeout=180)
        version = json.loads(package.read_text())["version"]
        if version != expected["mathjax-full"]:
            raise RuntimeError("Unexpected MathJax version")
        # Validate a real conversion before admitting the runtime into a distributable.
        probe = subprocess.run([str(node), str(destination / "tex-svg.cjs")], input='[{"latex":"x^2","display":false}]', text=True, capture_output=True, check=True, timeout=20)
        if "<svg" not in json.loads(probe.stdout)[0]["svg"]:
            raise RuntimeError("MathJax smoke conversion failed")
        shutil.copy2(node, binary)
        shutil.copy2(root / "LICENSE", destination / "NODE-LICENSE")
        complete.write_text(json.dumps(expected, indent=2) + "\n")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--destination", type=Path, default=Path(__file__).resolve().parents[1] / "third_party/mathjax")
    parser.add_argument("--allow-unavailable", action="store_true", help="Allow core/UI development builds without network; distributions remain strict")
    args = parser.parse_args()
    try:
        prepare(args.destination.resolve())
    except Exception as error:
        parser.exit(0 if args.allow_unavailable else 1, f"Scalar math runtime preparation failed: {error}\n" + ("Text and canvas are available; real LaTeX remains unavailable in this development build.\n" if args.allow_unavailable else ""))
