#!/usr/bin/env bash
set -euo pipefail
script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
source "$script_dir/release-common.sh"
rtv_home; rtv_managed
[ "${1:-}" = --all ] || rtv_fail 'Uso: bash uninstall-native.sh --all (encerre o aplicativo primeiro).'
# Resolved prefix was checked within HOME, must be a dedicated managed tree.
[ "$rtv_prefix" != / ] && [ "$rtv_prefix" != "$HOME" ] || rtv_fail 'Diretorio inseguro.'
printf 'Removendo somente a instalacao gerenciada: %s\n' "$rtv_prefix"
rm -rf -- "$rtv_prefix"
printf 'Configuracoes/favoritos XDG e instalacao v0.5 preservados.\n'
