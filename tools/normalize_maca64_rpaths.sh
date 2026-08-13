#!/usr/bin/env bash
set -euo pipefail
mex="${1:?usage: normalize_maca64_rpaths.sh <mex file>}"
echo "== before =="; otool -l "$mex" | awk '/LC_RPATH/{getline;getline;print "  rpath: " $2}'
while read -r rp; do
  [ -n "$rp" ] || continue
  echo "deleting absolute rpath: $rp"
  install_name_tool -delete_rpath "$rp" "$mex" || true
done < <(otool -l "$mex" | awk '/LC_RPATH/{getline;getline;print $2}' | grep -E '^/' || true)
add_rpath_if_missing() {
  local p="$1"
  otool -l "$mex" | awk '/LC_RPATH/{getline;getline;print $2}' | grep -qxF "$p" || install_name_tool -add_rpath "$p" "$mex"
}
add_rpath_if_missing "@executable_path"
add_rpath_if_missing "@executable_path/../../extern/bin/maca64"
codesign --force --sign - "$mex"        # install_name_tool voids the signature; arm64 requires a valid one
echo "== after =="; otool -l "$mex" | awk '/LC_RPATH/{getline;getline;print "  rpath: " $2}'
otool -l "$mex" | awk '/LC_RPATH/{getline;getline;print $2}' | grep -qE '^/' && { echo "ERROR: absolute rpath survived" >&2; exit 1; } || echo "OK: rpaths are version-independent"
