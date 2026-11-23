#!/usr/bin/env bash
# Diagnose 1080p30 presentation problems while X11 and LightDM stay running.
# Non-destructive: no changes to HDMI mode, services or GStreamer configuration.
set -Eeuo pipefail

VIDEO="/tmp/raspberry-tv-1080p30.h264"
if [[ $# -gt 0 ]]; then VIDEO="$1"; fi
if [[ ! -s "$VIDEO" ]]; then
  echo "Arquivo inexistente ou vazio: $VIDEO" >&2
  echo "Primeiro execute: bash scripts/test-h264-fullhd.sh" >&2
  exit 1
fi
for element in h264parse v4l2h264dec glimagesink fpsdisplaysink; do
  if ! gst-inspect-1.0 "$element" >/dev/null 2>&1; then
    echo "Elemento GStreamer ausente: $element" >&2
    exit 2
  fi
done
if ! command -v gst-launch-1.0 >/dev/null 2>&1; then
  echo "gst-launch-1.0 nao encontrado" >&2
  exit 2
fi
export DISPLAY=:0
LOGDIR="$HOME/raspberry-tv-1080p-diagnostics"
mkdir -p "$LOGDIR"

echo "======= A. FPS REAL: video H264 -> V4L2 -> glimagesink ========="
echo "Observe os campos 'rendered', 'dropped', 'current' e 'average'."
echo "Log: $LOGDIR/fps-gl-1080p.log"
if gst-launch-1.0 -e -v \
  filesrc location="$VIDEO" ! h264parse ! v4l2h264dec ! \
  fpsdisplaysink video-sink=glimagesink text-overlay=false \
                 fps-update-interval=500 sync=true \
  > "$LOGDIR/fps-gl-1080p.log" 2>&1; then
  fps_status=0
else
  fps_status=$?
fi
grep -E 'last-message =|buffers are being dropped|large amount of buffers|Execution ended|ERRO:|ERROR:' \
  "$LOGDIR/fps-gl-1080p.log" | tail -25 || true
echo "Resultado do teste A (exit): $fps_status"

echo
echo "======= B. PROVA DE NEGOCIACAO DMA-BUF -> OPENGL ========="
echo "Uma falha aqui pode indicar incompatibilidade do driver/importador."
echo "Log: $LOGDIR/dmabuf-gl-1080p.log"
if gst-launch-1.0 -e -v \
  filesrc location="$VIDEO" ! h264parse ! v4l2h264dec ! \
  'video/x-raw(memory:DMABuf),format=(string)DMA_DRM,drm-format=(string)YU12' ! \
  glimagesink sync=true \
  > "$LOGDIR/dmabuf-gl-1080p.log" 2>&1; then
  dmabuf_status=0
else
  dmabuf_status=$?
fi
grep -E 'caps =|Execution ended|not-negotiated|not-linked|ERRO:|ERROR:|buffers are being dropped|large amount of buffers' \
  "$LOGDIR/dmabuf-gl-1080p.log" | tail -35 || true
echo "Resultado do teste B (exit): $dmabuf_status"

echo
echo "======= DIAGNOSTICO FINAL ========="
echo "Logs preservados em: $LOGDIR"
echo "FPS GL exit: $fps_status; GL DMA-BUF exit: $dmabuf_status"
echo "Um pipeline bem-sucedido nao prova 30 FPS estaveis ou zero-copy completo."
echo "Se o teste B falhou, nao alterar o LightDM/XFCE nem forcar KMS nesta etapa."
