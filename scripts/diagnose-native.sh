#!/usr/bin/env bash
# Read-only diagnostics. No apt, sudo, service changes or stream URLs.
set -u
printf 'Raspberry TV Native diagnostics\n'
uname -sm
printf 'Session: %s; display set: %s\n' "${XDG_SESSION_TYPE:-unknown}" "$([ -n "${DISPLAY:-}" ] && printf yes || printf no)"
missing=0
for tool in cmake ctest c++ pkg-config gst-inspect-1.0 python3; do
  if ! command -v "$tool" >/dev/null; then printf 'MISSING tool: %s\n' "$tool"; missing=1; fi
done
if command -v pkg-config >/dev/null; then
  for package in sdl2 SDL2_ttf gstreamer-1.0 gstreamer-video-1.0; do
    if pkg-config --exists "$package"; then printf '%s: %s\n' "$package" "$(pkg-config --modversion "$package")"
    else printf 'MISSING development package: %s\n' "$package"; missing=1; fi
  done
fi
if command -v gst-inspect-1.0 >/dev/null; then
  for plugin in playbin glimagesink autoaudiosink; do
    if gst-inspect-1.0 "$plugin" >/dev/null 2>&1; then printf 'Required plugin OK: %s\n' "$plugin"
    else printf 'MISSING required plugin: %s\n' "$plugin"; missing=1; fi
  done
  for plugin in souphttpsrc hlsdemux hlsdemux2 tsdemux h264parse v4l2h264dec avdec_h264 rtspsrc wavparse alsasink pulsesink pipewiresink; do
    if gst-inspect-1.0 "$plugin" >/dev/null 2>&1; then printf 'Available: %s\n' "$plugin"
    else printf 'Unavailable (format/device dependent): %s\n' "$plugin"; fi
  done
fi
if command -v vcgencmd >/dev/null; then vcgencmd get_throttled; vcgencmd measure_temp; fi
if [ -r /sys/firmware/devicetree/base/model ]; then tr -d '\0' < /sys/firmware/devicetree/base/model; printf '\n'; fi
if command -v free >/dev/null; then free -m; fi
if command -v aplay >/dev/null; then aplay -l 2>/dev/null || true; fi
if command -v xrandr >/dev/null && [ -n "${DISPLAY:-}" ]; then xrandr --current 2>/dev/null | awk '/ connected|\*/' || true; fi
printf 'Plugin availability does not confirm audible audio or visible video.\n'
exit "$missing"
