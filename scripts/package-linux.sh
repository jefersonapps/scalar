#!/usr/bin/env bash
# Build a Debian package in a private directory, validate it completely, then
# publish it atomically.  A package interrupted during compression is never
# placed in build-package/packages for distribution.
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${1:-"$root/build-package"}"
artifacts_dir="$build_dir/packages"
staging_dir="$(mktemp -d "${TMPDIR:-/tmp}/scalar-deb.XXXXXX")"
trap 'rm -rf "$staging_dir"' EXIT

cmake -S "$root" -B "$build_dir" -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF
cmake --build "$build_dir" --parallel
cpack --config "$build_dir/CPackConfig.cmake" -G DEB -B "$staging_dir"

mapfile -d '' packages < <(find "$staging_dir" -maxdepth 1 -type f -name '*.deb' -print0)
if (( ${#packages[@]} != 1 )); then
    printf 'Expected one Debian package, found %s.\n' "${#packages[@]}" >&2
    exit 1
fi

package="${packages[0]}"
# --contents fully decompresses data.tar, catching truncated archives before
# they are copied to the user-visible artifacts directory.
dpkg-deb --info "$package" >/dev/null
dpkg-deb --contents "$package" >/dev/null
# Extract the real payload too; archive listing alone does not exercise writing
# files or confirm that the desktop launcher points to the installed command.
dpkg-deb --extract "$package" "$staging_dir/extracted"
test -x "$staging_dir/extracted/usr/bin/scalar-whiteboard"
test ! -e "$staging_dir/extracted/usr/bin/scalar"
grep -Fxq 'Exec=scalar-whiteboard' "$staging_dir/extracted/usr/share/applications/scalar.desktop"

mkdir -p "$artifacts_dir"
name="$(basename "$package")"
temporary="$artifacts_dir/.${name}.partial"
cp "$package" "$temporary"
dpkg-deb --contents "$temporary" >/dev/null
mv -f "$temporary" "$artifacts_dir/$name"
install -m 755 "$root/scripts/install-linux.sh" "$artifacts_dir/install-scalar.sh"

printf 'Package ready: %s\n' "$artifacts_dir/$name"
sha256sum "$artifacts_dir/$name"
