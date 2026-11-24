#!/usr/bin/env bash
# Benchmark de estágios: H264 1080p30 no Raspberry Pi sem mudar serviços.
# Não modifica Xorg/LightDM, HDMI, módulos ou autostart.
set -Eeuo pipefail
VIDEO="${1:-/tmp/raspberry-tv-1080p30.h264}"
[[ -s "$VIDEO" ]] || { echo "Video ausente: $VIDEO" >&2; exit 1; }
for name in h264parse v4l2h264dec fpsdisplaysink fakesink glimagesink videotestsrc; do
  gst-inspect-1.0 "$name" >/dev/null 2>&1 || { echo "Plugin ausente: $name" >&2; exit 1; }
done
export DISPLAY="${DISPLAY:-:0}"
OUT="$HOME/raspberry-tv-1080p-diagnostics"
mkdir -p "$OUT"
echo "==== AMBIENTE ===="
date -Is
echo "DISPLAY=$DISPLAY"
if command -v xrandr >/dev/null 2>&1; then xrandr --current | head -9; fi
if command -v vcgencmd >/dev/null 2>&1; then vcgencmd get_throttled || true; vcgencmd measure_temp || true; fi
if [[ -r /sys/class/thermal/thermal_zone0/temp ]]; then cat /sys/class/thermal/thermal_zone0/temp; fi
echo
run_test() {
  local label="$1"; shift
  local logfile="$OUT/${label}.log"
  echo "===== $label ====="
  echo "Log: $logfile"
  local status=0
  local started ended
  started=$(date +%s%3N)
  if timeout --signal=INT --kill-after=3s 30s gst-launch-1.0 -e -v "$@" \
    >"$logfile" 2>&1; then
    status=0
  else
    status=$?
  fi
  ended=$(date +%s%3N)
  echo "Tempo total medido: $((ended - started)) ms" | tee -a "$logfile"
  grep -E "last-message =|Execution ended|rendered:|dropped:|Tempo total medido|ERRO:|ERROR:|not-negotiated|timed out|Command exited" "$logfile" | tail -8 || true
  echo "Status: $status"
  echo
}
echo "Execute sem outra instancia da Raspberry TV ocupando a GPU."
echo "Teste 1: so decodificacao V4L2. sync=false mede vazao maxima, nao fluidez."
run_test 01-decoder-fakesink \
  filesrc location="$VIDEO" ! h264parse ! v4l2h264dec ! \
  fpsdisplaysink video-sink=fakesink text-overlay=false fps-update-interval=250 sync=false
echo "Teste 2: OpenGL sem sincronizar relogio; revela custo bruto de upload/apresentacao."
run_test 02-decoder-gl-nosync \
  filesrc location="$VIDEO" ! h264parse ! v4l2h264dec ! \
  fpsdisplaysink video-sink=glimagesink text-overlay=false fps-update-interval=250 sync=false
echo "Teste 3: decoder e GL em threads separadas, sync=true."
run_test 03-decoder-queue-gl-sync \
  filesrc location="$VIDEO" ! h264parse ! v4l2h264dec ! \
  queue max-size-buffers=4 max-size-bytes=0 max-size-time=0 ! \
  fpsdisplaysink video-sink=glimagesink text-overlay=false fps-update-interval=250 sync=true
echo "Teste 4: GL com 90 quadros sinteticos I420, sem H264 nem V4L2."
run_test 04-synthetic-i420-gl-sync \
  videotestsrc pattern=black num-buffers=90 ! \
  "video/x-raw,format=(string)I420,width=(int)1920,height=(int)1080,framerate=(fraction)30/1" ! \
  fpsdisplaysink video-sink=glimagesink text-overlay=false fps-update-interval=250 sync=true
echo "==== FIM ===="
if command -v vcgencmd >/dev/null 2>&1; then vcgencmd get_throttled || true; vcgencmd measure_temp || true; fi
echo "Arquivos de diagnostico: $OUT"
echo "Nao interpretar sync=false como video fluido; comparar rendered, dropped e tempos."
