#!/usr/bin/env bash
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
binary=${RASPBERRY_TV_BIN:-"$root/bin/raspberry-tv-native"}
if [ ! -x "$binary" ]; then binary="$root/build-native/raspberry-tv-native"; fi
if [ ! -x "$binary" ]; then printf 'Compile primeiro: consulte docs/INSTALLATION.md\n' >&2; exit 1; fi
case "${1:-}" in --help|--version|--check-playlist) ;; *)
  if [ -z "${DISPLAY:-}" ]; then printf 'Execute na sessao XFCE/X11 do usuario; DISPLAY nao definido.\n' >&2; exit 1; fi ;;
esac
# Avoid accidental token disclosure through inherited verbose Gst debug logging.
export GST_DEBUG=0
export RASPBERRY_TV_SPOTIFY_HELPER="${RASPBERRY_TV_SPOTIFY_HELPER:-$root/scripts/desktop-launcher.py}"
export RASPBERRY_TV_SPOTIFY_PROJECT="${RASPBERRY_TV_SPOTIFY_PROJECT:-$root}"
exec "$binary" "$@"
