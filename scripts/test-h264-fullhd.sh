#!/usr/bin/env bash
# Raspberry TV: local 1080p30 H.264 diagnostic. Does not modify the display mode.
set -Eeuo pipefail
FILE="/tmp/raspberry-tv-1080p30.h264"
for dep in ffmpeg gst-launch-1.0 gst-inspect-1.0; do
  if ! command -v "$dep" >/dev/null 2>&1; then
    echo "Comando ausente: $dep" >&2
    exit 1
  fi
done
if ! gst-inspect-1.0 h264parse >/dev/null 2>&1 ||
   ! gst-inspect-1.0 v4l2h264dec >/dev/null 2>&1; then
  echo "Faltam h264parse e/ou v4l2h264dec no GStreamer" >&2
  exit 1
fi
if [[ ! -s "$FILE" ]]; then
  echo "Gerando 3 segundos de H.264 1920x1080 a 30 FPS (CPU: libx264) ..."
  ffmpeg -hide_banner -loglevel error \
    -f lavfi -i 'testsrc2=size=1920x1080:rate=30' \
    -t 3 -an -c:v libx264 -preset ultrafast -threads 2 \
    -pix_fmt yuv420p -profile:v baseline -level:v 4.0 \
    -f h264 -y "$FILE"
fi
echo "===== ARQUIVO FULL HD ====="
ls -lh "$FILE"
echo "===== DECODE V4L2 1080P30 ====="
time gst-launch-1.0 -v -e \
  filesrc location="$FILE" ! h264parse ! v4l2h264dec ! \
  fakesink sync=false

if [[ "${1:-}" == "--show" ]]; then
  if ! gst-inspect-1.0 glimagesink >/dev/null 2>&1; then
    echo "glimagesink ausente. Tente instalar gstreamer1.0-gl." >&2
    exit 2
  fi
  echo "===== EXIBICAO HDMI (X11) ====="
  DISPLAY="${DISPLAY:-:0}" gst-launch-1.0 -v -e \
    filesrc location="$FILE" ! h264parse ! v4l2h264dec ! \
    glimagesink sync=true
fi
