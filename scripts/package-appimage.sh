#!/usr/bin/env bash
# Build in an older userspace by default. --local is for development only.
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
mode="${1:-}"
case "$mode" in
    -h|--help)
        cat <<'EOF'
Uso: bash scripts/package-appimage.sh [--local]

Sem argumentos: compila em Debian 12 com Docker ou Podman (glibc >= 2.36).
--local: usa as dependências locais; exige a glibc da máquina de compilação.
Saída: build-package/packages/Scalar-<versão>-x86_64.AppImage
Variáveis: SCALAR_CONTAINER_ENGINE=docker|podman, SCALAR_JOBS=4.
O navegador integrado, Qt e matemática offline são incluídos no pacote.
EOF
        exit 0 ;;
    ''|--local|--container) ;;
    *) printf 'Argumento desconhecido: %s\n' "$mode" >&2; exit 2 ;;
esac
[[ $# -le 1 ]] || { echo 'Use --help para consultar os argumentos.' >&2; exit 2; }
[[ "$(uname -s)" == Linux && "$(uname -m)" == x86_64 ]] || {
    echo 'Este script suporta Linux x86_64; não faz compilação cruzada.' >&2; exit 1;
}
if [[ -z "$mode" ]]; then
    engine="${SCALAR_CONTAINER_ENGINE:-}"
    if [[ -z "$engine" ]]; then
        for candidate in podman docker; do
            if command -v "$candidate" >/dev/null; then engine="$candidate"; break; fi
        done
    fi
    [[ "$engine" == docker || "$engine" == podman ]] && command -v "$engine" >/dev/null || {
        echo 'Instale Docker ou Podman para compilar na base compatível Debian 12.' >&2
        echo 'No Mint/Ubuntu: sudo apt install podman' >&2
        echo 'Para usar as dependências locais: bash scripts/package-appimage.sh --local' >&2
        exit 1
    }
    "$engine" build -t scalar-appimage-bookworm -f "$root/packaging/appimage/Dockerfile" "$root/packaging/appimage"
    user_args=(--user "$(id -u):$(id -g)")
    [[ "$engine" != podman ]] || user_args+=(--userns=keep-id)
    # WebKit creates its own user/mount namespaces during the mandatory test.
    # Permit that nested sandbox; no --privileged or host device access is used.
    exec "$engine" run --rm "${user_args[@]}" \
        --security-opt seccomp=unconfined --security-opt apparmor=unconfined \
        -e HOME=/tmp/scalar-builder -e SCALAR_JOBS="${SCALAR_JOBS:-4}" \
        -v "$root:/src" scalar-appimage-bookworm bash scripts/package-appimage.sh --container
fi
for tool in cmake ninja python3 curl file readelf pkg-config qmake6 desktop-file-validate flock timeout g++ dpkg-query convert; do
    command -v "$tool" >/dev/null || { echo "Ferramenta ausente: $tool" >&2; exit 1; }
done
python3 -c 'import sys; assert sys.version_info >= (3,12), "Python >= 3.12 necessário"'
pkg-config --exists webkit2gtk-4.1 || { echo 'Instale libwebkit2gtk-4.1-dev.' >&2; exit 1; }
if ! command -v xvfb-run >/dev/null && [[ -z "${DISPLAY:-}" ]]; then
    echo 'Instale xvfb e xauth para testar o navegador antes de publicar.' >&2; exit 1
fi
build="$root/build-package/appimage-${mode#--}"
tools_dir="$root/build-package/appimage-tools"
output="$root/build-package/packages"
mkdir -p "$build" "$tools_dir" "$output"
# Concurrent runs must not share a CMake cache or publish over one another.
exec 9>"$root/build-package/appimage.lock"
flock -n 9 || { echo 'Já existe uma geração de AppImage em andamento.' >&2; exit 1; }
stage="$(mktemp -d "$build/stage.XXXXXX")"
cleanup() {
    local status=$?
    if (( status == 0 )); then
        rm -rf -- "$stage"
    else
        printf '\nGeração interrompida. Arquivos de diagnóstico preservados em: %s\n' "$stage" >&2
    fi
}
trap cleanup EXIT
appdir="$stage/Scalar.AppDir"
download() {
    local url="$1" destination="$2"
    if [[ ! -s "$destination" ]]; then
        curl -fL --retry 3 --connect-timeout 30 "$url" -o "$destination.partial"
        chmod +x "$destination.partial"
        mv "$destination.partial" "$destination"
    fi
}
download https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage "$tools_dir/linuxdeploy-x86_64.AppImage"
download https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage "$tools_dir/linuxdeploy-plugin-qt-x86_64.AppImage"
download https://raw.githubusercontent.com/linuxdeploy/linuxdeploy-plugin-gtk/master/linuxdeploy-plugin-gtk.sh "$tools_dir/linuxdeploy-plugin-gtk.sh"
export APPIMAGE_EXTRACT_AND_RUN=1
export QMAKE="$(command -v qmake6)" QML_SOURCES_PATHS="$root/qml"
export EXTRA_QT_MODULES='svg;sql' DEPLOY_GTK_VERSION=3
cmake -S "$root" -B "$build" -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF
cmake --build "$build" --parallel "${SCALAR_JOBS:-4}"
DESTDIR="$appdir" cmake --install "$build" --prefix /usr
# linuxdeploy rejects nonstandard icon sizes (the source artwork is 1254px).
# Normalize only the staged copy, keeping the desktop entry's hashed icon name.
for icon in "$appdir"/usr/share/pixmaps/scalar-*.png; do
    convert "$icon" -resize 512x512 -background none -gravity center \
        -extent 512x512 "$stage/icon-512.png"
    install -m644 "$stage/icon-512.png" "$icon"
done
libdir="$(pkg-config --variable=libdir webkit2gtk-4.1)"
helpers="$libdir/webkit2gtk-4.1"
[[ -x "$helpers/WebKitWebProcess" && -x "$helpers/WebKitNetworkProcess" ]] || {
    echo "Processos auxiliares WebKit não encontrados em $helpers" >&2; exit 1;
}
mkdir -p "$appdir/usr/lib" "$appdir/usr/libexec/gstreamer-1.0"
cp -a "$helpers" "$appdir/usr/lib/webkit2gtk-4.1"
cp -a "$libdir/gio" "$appdir/usr/lib/"
mkdir -p "$appdir/usr/lib/gstreamer-1.0"
# Match the container's media set even if the developer machine has unrelated
# hardware encoders and third-party plugins installed.
for package in libgstreamer1.0-0 gstreamer1.0-plugins-base gstreamer1.0-plugins-good; do
    files="$(dpkg-query -L "$package")"
    while IFS= read -r plugin; do
        case "$plugin" in
            "$libdir/gstreamer-1.0/"*.so) cp -L "$plugin" "$appdir/usr/lib/gstreamer-1.0/" ;;
        esac
    done <<< "$files"
done
cp -a "$libdir/gstreamer1.0/gstreamer-1.0/gst-plugin-scanner" "$appdir/usr/libexec/gstreamer-1.0/"
install -m755 /usr/bin/bwrap /usr/bin/xdg-dbus-proxy "$appdir/usr/bin/"
# Keep the browser host in libexec: only scalar-whiteboard is a desktop entry.
mv "$appdir/usr/libexec/scalar/scalar-webkit-host" "$appdir/usr/libexec/scalar/scalar-webkit-host.bin"
install -m755 "$root/packaging/appimage/webkit-launcher" "$appdir/usr/libexec/scalar/scalar-webkit-host"
install -m644 "$root/packaging/appimage/webkit-launcher.cjs" "$appdir/usr/libexec/scalar/"
install -m644 "$root/packaging/appimage/runtime-env.sh" "$appdir/usr/libexec/scalar/"
"$tools_dir/linuxdeploy-x86_64.AppImage" --appdir "$appdir" \
    --executable "$appdir/usr/bin/scalar-whiteboard" \
    --deploy-deps-only "$appdir/usr/libexec" \
    --deploy-deps-only "$appdir/usr/share/scalar/math/node" \
    --library "$(g++ -print-file-name=libstdc++.so.6)" \
    --library "$(g++ -print-file-name=libgcc_s.so.1)" \
    --desktop-file "$appdir/usr/share/applications/scalar.desktop" \
    --icon-file "$appdir"/usr/share/pixmaps/scalar-*.png \
    --plugin qt --plugin gtk --custom-apprun "$root/packaging/appimage/AppRun"
python3 "$root/scripts/validate-appimage.py" "$appdir" "$helpers" "$mode"
# Smoke-test the staged application with isolated settings, without touching the
# user's library. Validation is required before the artifact becomes distributable.
mkdir -p "$stage/home" "$stage/runtime"
chmod 700 "$stage/runtime"
ui_test=(env HOME="$stage/home" XDG_CONFIG_HOME="$stage/home/config" \
    XDG_DATA_HOME="$stage/home/data" XDG_CACHE_HOME="$stage/home/cache" \
    XDG_RUNTIME_DIR="$stage/runtime" QT_QPA_PLATFORM=xcb \
    QT_QUICK_BACKEND=software timeout 60 "$appdir/AppRun" --smoke-test)
browser_test=(python3 "$root/scripts/test-appimage-browser.py" "$appdir")
if command -v xvfb-run >/dev/null; then
    timeout --kill-after=5 90 xvfb-run -a "${ui_test[@]}"
    timeout --kill-after=5 90 xvfb-run -a "${browser_test[@]}"
elif [[ -n "${DISPLAY:-}" ]]; then
    "${ui_test[@]}"
    "${browser_test[@]}"
else
    echo 'Instale xvfb e xauth: o teste obrigatório do navegador precisa de X11.' >&2
    exit 1
fi
version="$(sed -n 's/^project(Scalar VERSION \([^ ]*\).*/\1/p' "$root/CMakeLists.txt")"
[[ -n "$version" ]] || { echo 'Versão do projeto não encontrada.' >&2; exit 1; }
name="Scalar-$version-x86_64.AppImage"
chmod -R a+rX "$appdir"
export LDAI_OUTPUT="$stage/$name"
"$tools_dir/linuxdeploy-x86_64.AppImage" --appdir "$appdir" --output appimage
[[ -s "$stage/$name" ]] || { echo 'AppImage não foi gerado.' >&2; exit 1; }
chmod +x "$stage/$name"
# The runtime must be able to extract the final SquashFS without FUSE.
(cd "$stage" && "./$name" --appimage-extract >/dev/null)
test -x "$stage/squashfs-root/usr/bin/scalar-whiteboard"
test -x "$stage/squashfs-root/usr/lib/webkit2gtk-4.1/WebKitWebProcess"
cp "$stage/$name" "$output/.$name.partial"
mv -f "$output/.$name.partial" "$output/$name"
(cd "$output" && sha256sum "$name" > "$name.sha256")
printf '\nAppImage pronto: %s\n' "$output/$name"
printf 'Base de compilação: %s\n' "$(getconf GNU_LIBC_VERSION)"
