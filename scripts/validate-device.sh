#!/usr/bin/env bash
# Bounded, read-only sampling. Does not launch a stream, reboot or stress CPU.
set -euo pipefail
printf 'Amostra curta: alimentacao, temperatura e memoria (6 x 2 s).\n'
for ((i=1;i<=6;i++)); do
  date -Is
  if command -v vcgencmd >/dev/null; then vcgencmd get_throttled; vcgencmd measure_temp; fi
  awk '/MemAvailable|MemFree/ {print}' /proc/meminfo
  sleep 2
done
printf 'Agora execute os testes fisicos descritos em docs/RELEASE_CHECKLIST.md.\n'
