export LD_LIBRARY_PATH="$APPDIR/usr/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
# New Mesa/LLVM drivers may require a newer C++ runtime than our build base.
# Prefer the host runtime only if it supplies the bundle's highest GLIBCXX ABI.
# Preloading also overrides the RPATH set by linuxdeploy, unlike library paths.
scalar_bundled_cpp="$APPDIR/usr/lib/libstdc++.so.6"
scalar_ldconfig="$(command -v ldconfig || true)"
[[ -n "$scalar_ldconfig" ]] || scalar_ldconfig=/sbin/ldconfig
if [[ -r "$scalar_bundled_cpp" && -x "$scalar_ldconfig" ]]; then
    scalar_host_cpp="$("$scalar_ldconfig" -p 2>/dev/null | awk '$1 == "libstdc++.so.6" && /x86-64/ {print $NF; exit}')"
    scalar_cpp_abi="$(LC_ALL=C grep -aoE 'GLIBCXX_[0-9]+\.[0-9]+(\.[0-9]+)?' "$scalar_bundled_cpp" | sort -Vu | tail -n1)"
    if [[ -n "$scalar_cpp_abi" && -r "$scalar_host_cpp" ]] && \
        LC_ALL=C grep -aFq "$scalar_cpp_abi" "$scalar_host_cpp"; then
        export LD_PRELOAD="$scalar_host_cpp${LD_PRELOAD:+:$LD_PRELOAD}"
    fi
fi
unset scalar_bundled_cpp scalar_ldconfig scalar_host_cpp scalar_cpp_abi
for hook in "$APPDIR"/apprun-hooks/*.sh; do
    [[ ! -f "$hook" ]] || source "$hook"
done
export GIO_MODULE_DIR="$APPDIR/usr/lib/gio/modules"
export GST_PLUGIN_SYSTEM_PATH_1_0="$APPDIR/usr/lib/gstreamer-1.0"
export GST_PLUGIN_SCANNER="$APPDIR/usr/libexec/gstreamer-1.0/gst-plugin-scanner"
export WEBKIT_INJECTED_BUNDLE_PATH="$APPDIR/usr/lib/webkit2gtk-4.1/injected-bundle"
export SCALAR_MATH_RUNTIME="$APPDIR/usr/share/scalar/math"
