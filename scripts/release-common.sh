#!/usr/bin/env bash
# Sourced only by the explicit per-user release management commands.
set -euo pipefail
rtv_fail() { printf '%s\n' "$*" >&2; exit 1; }
rtv_home() {
  [ "$(id -u)" -ne 0 ] || rtv_fail 'Execute como usuario comum, sem sudo.'
  rtv_prefix=$(realpath -m -- "${RASPBERRY_TV_HOME:-$HOME/.local/opt/raspberry-tv}")
  case "$rtv_prefix" in "$HOME"/*) ;; *) rtv_fail 'O diretorio gerenciado deve ficar dentro de HOME.' ;; esac
  [ "$rtv_prefix" != "$HOME" ] || rtv_fail 'Diretorio invalido.'
}
rtv_managed() {
  [ -f "$rtv_prefix/.raspberry-tv-managed" ] || rtv_fail 'Diretorio sem marcador de instalacao gerenciada.'
  [ "$(cat -- "$rtv_prefix/.raspberry-tv-managed")" = raspberry-tv-managed-v1 ] || rtv_fail 'Marcador invalido.'
}
rtv_release_target() {
  local target
  target=$(readlink -f -- "$1") || return 1
  case "$target" in "$rtv_prefix/releases/"*) ;; *) return 1 ;; esac
  [ -f "$target/.raspberry-tv-release" ] || return 1
  [ "$(cat -- "$target/.raspberry-tv-release")" = raspberry-tv-managed-release-v1 ] || return 1
  printf '%s' "$target"
}
rtv_link() {
  local target=$1 link=$2 temp="$2.new.$$"
  [ ! -e "$link" ] || [ -L "$link" ] || rtv_fail "Recusando substituir arquivo/diretorio: $link"
  ln -s -- "$target" "$temp"
  mv -Tf -- "$temp" "$link"
}
