#!/usr/bin/env bash
set -euo pipefail
script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
source "$script_dir/release-common.sh"
rtv_home; rtv_managed
current=$(rtv_release_target "$rtv_prefix/current") || rtv_fail 'Release atual invalida.'
previous=$(rtv_release_target "$rtv_prefix/previous") || rtv_fail 'Nao ha release anterior gerenciada; execute a v0.5 preservada.'
rtv_link "$previous" "$rtv_prefix/current"
rtv_link "$current" "$rtv_prefix/previous"
printf 'Rollback realizado. Reabra o aplicativo manualmente. Dados XDG preservados.\n'
