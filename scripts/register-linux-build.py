#!/usr/bin/env python3
"""Register a development build in the current user's Linux application menu."""

import argparse
import hashlib
import os
from pathlib import Path
import shutil
import subprocess
import sys


def desktop_string(value):
    return str(value).replace("\\", "\\\\").replace("\n", "\\n").replace("\r", "\\r").replace("\t", "\\t")


def main():
    root = Path(__file__).resolve().parent.parent
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("binary", nargs="?", type=Path, default=root / "build/scalar")
    args = parser.parse_args()
    binary = args.binary.resolve()
    if not binary.is_file() or not os.access(binary, os.X_OK):
        parser.error(f"Executavel nao encontrado: {binary}. Compile o aplicativo primeiro.")
    if not sys.platform.startswith("linux"):
        parser.error("Este comando e exclusivo para Linux.")

    data = Path(os.environ.get("XDG_DATA_HOME") or Path.home() / ".local/share").resolve()
    artwork = (root / "icon.png").read_bytes()
    digest = hashlib.sha256(artwork).hexdigest()
    icon = data / "pixmaps" / f"scalar-{digest}.png"
    icon.parent.mkdir(parents=True, exist_ok=True)
    icon.write_bytes(artwork)
    # Exec has its own escaping rules in addition to desktop-string escaping.
    command = str(binary).replace("%", "%%")
    for character in ('\\', '"', '`', '$'):
        command = command.replace(character, "\\" + character)
    entry = (root / "packaging/scalar.desktop").read_text()
    entry = entry.replace("Exec=scalar-whiteboard", 'Exec=' + desktop_string('"' + command + '"'))
    entry = entry.replace("Icon=scalar\n", f"Icon={desktop_string(icon)}\n")
    applications = data / "applications"
    applications.mkdir(parents=True, exist_ok=True)
    launcher = applications / "scalar.desktop"
    launcher.write_text(entry)
    if shutil.which("update-desktop-database"):
        subprocess.run(["update-desktop-database", str(applications)], check=True)
    print(f"Menu Scalar atualizado para: {binary}")
    print(f"Icone: {icon}")
    print("Feche e reabra o Scalar. Se o painel mantiver o icone antigo, desafixe e fixe novamente.")


if __name__ == "__main__":
    main()
