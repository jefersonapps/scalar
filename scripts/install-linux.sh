#!/usr/bin/env bash
# Interactive companion to the .deb; run as the desktop user, without sudo.
set -euo pipefail

if (( EUID == 0 )); then
    printf 'Execute este assistente como seu usuario, sem sudo.\n' >&2
    exit 1
fi

directory="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
if (( $# > 1 )); then
    printf 'Uso: %s [pacote.deb]\n' "$0" >&2
    exit 1
fi
if (( $# == 1 )); then
    package="$(realpath -- "$1")"
else
    shopt -s nullglob
    packages=("$directory"/scalar*.deb)
    if (( ${#packages[@]} != 1 )); then
        printf 'Informe o caminho de um unico pacote Scalar .deb.\n' >&2
        exit 1
    fi
    package="${packages[0]}"
fi
test -f "$package"

graphical=false
if [[ -n "${DISPLAY:-}${WAYLAND_DISPLAY:-}" ]] && command -v zenity >/dev/null && command -v pkexec >/dev/null; then
    graphical=true
fi
log="$(mktemp)"
trap 'rm -f -- "$log"' EXIT
if "$graphical"; then
    zenity --question --title='Instalar Scalar' --text='Instalar o Scalar e suas dependencias?' || exit 0
    if ! pkexec /usr/bin/apt-get install -y "$package" >"$log" 2>&1; then
        zenity --text-info --title='Falha na instalacao do Scalar' --filename="$log"
        exit 1
    fi
else
    sudo /usr/bin/apt-get install "$package"
fi

run=false
desktop=false
if "$graphical"; then
    # Both choices are initially checked and only applied after confirmation.
    if ! choices="$(zenity --list --checklist --title='Instalacao concluida' \
        --text='Escolha as acoes ao concluir:' --column='Selecionar' --column='Acao' \
        --separator='|' --height=260 --width=420 --ok-label='Concluir' \
        TRUE 'Executar o Scalar' TRUE 'Criar atalho na area de trabalho')"; then
        exit 0
    fi
    [[ "$choices" != *'Executar o Scalar'* ]] || run=true
    [[ "$choices" != *'Criar atalho na area de trabalho'* ]] || desktop=true
else
    read -r -p 'Criar atalho na area de trabalho? [S/n] ' answer
    [[ "$answer" == [nN]* ]] || desktop=true
    read -r -p 'Executar o Scalar? [S/n] ' answer
    [[ "$answer" == [nN]* ]] || run=true
fi

if "$desktop"; then
    if command -v xdg-user-dir >/dev/null; then
        desktop_dir="$(xdg-user-dir DESKTOP)"
    else
        printf 'xdg-user-dir nao encontrado; nao foi possivel localizar a area de trabalho.\n' >&2
        exit 1
    fi
    if [[ -z "$desktop_dir" || "$desktop_dir" == "$HOME" ]]; then
        printf 'A area de trabalho esta desativada neste ambiente.\n' >&2
        exit 1
    fi
    mkdir -p -- "$desktop_dir"
    install -m 755 /usr/share/applications/scalar.desktop "$desktop_dir/Scalar.desktop"
    # GNOME may still ask the user to allow launching the desktop shortcut.
    if command -v gio >/dev/null; then
        gio set "$desktop_dir/Scalar.desktop" metadata::trusted true 2>/dev/null || true
    fi
fi
if "$run"; then
    nohup scalar-whiteboard >/dev/null 2>&1 &
fi
