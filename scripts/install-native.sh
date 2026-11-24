#!/usr/bin/env bash
# Explicit installation, optional; no autostart or service changes.
set -euo pipefail
umask 077
script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
source "$script_dir/release-common.sh"
rtv_home
source_dir=$(cd -- "$script_dir/.." && pwd)
[ -f "$source_dir/.raspberry-tv-release" ] || rtv_fail 'Use o pacote ARM64, nao o checkout de fontes.'
[ -x "$source_dir/bin/raspberry-tv-native" ] || rtv_fail 'Executavel ausente.'
[ "$(uname -m)" = aarch64 ] || rtv_fail 'Este pacote e ARM64.'
(cd "$source_dir" && sha256sum -c SHA256SUMS)
release=$(cat -- "$source_dir/RELEASE_ID")
[[ "$release" =~ ^1\.0\.0-rc1-[0-9a-f]{12}$ ]] || rtv_fail 'Identificador de release invalido.'
if [ -d "$rtv_prefix" ]; then rtv_managed; else
  mkdir -p -- "$rtv_prefix"
  printf 'raspberry-tv-managed-v1\n' > "$rtv_prefix/.raspberry-tv-managed"
fi
mkdir -p -- "$rtv_prefix/releases"
target="$rtv_prefix/releases/$release"
[ ! -e "$target" ] || rtv_fail 'Release ja instalada; nenhuma alteracao realizada.'
for link in current previous; do
  if [ -e "$rtv_prefix/$link" ] || [ -L "$rtv_prefix/$link" ]; then
    rtv_release_target "$rtv_prefix/$link" >/dev/null || rtv_fail 'Link existente nao pertence a esta instalacao.'
  fi
done
stage=$(mktemp -d "$rtv_prefix/releases/.install.XXXXXXXX")
cp -a -- "$source_dir/." "$stage/"
mv -T -- "$stage" "$target"
if [ -L "$rtv_prefix/current" ]; then
  old=$(rtv_release_target "$rtv_prefix/current")
  rtv_link "$old" "$rtv_prefix/previous"
fi
rtv_link "$target" "$rtv_prefix/current"
printf 'Instalado sem autostart. Execute:\nbash "%s/current/scripts/run-native.sh" --fullscreen\n' "$rtv_prefix"
