#!/usr/bin/env bash
# Raspberry TV: monitorar subtensao e throttling sem alterar configuracoes.
set -Eeuo pipefail

if ! command -v vcgencmd >/dev/null 2>&1; then
  echo "vcgencmd nao encontrado; este diagnostico requer firmware Raspberry Pi." >&2
  exit 2
fi

LOGDIR="$HOME/raspberry-tv-1080p-diagnostics"
mkdir -p "$LOGDIR"
LOGFILE="$LOGDIR/power-status.log"
SAMPLES=10
ACTIVE=0

echo "Auditoria de alimentacao: $SAMPLES amostras, 1 por segundo."
echo "Saida adicional: $LOGFILE"
echo "Amostra | horario | status | atual | temperatura | clock ARM"

for ((i=1; i<=SAMPLES; ++i)); do
  raw="$(vcgencmd get_throttled)"
  if [[ ! "$raw" =~ ^throttled=0x[0-9a-fA-F]+$ ]]; then
    echo "Resposta inesperada de vcgencmd: $raw" >&2
    exit 2
  fi
  hex="$(printf '%s\n' "$raw" | cut -d= -f2)"
  flags=$((hex))
  live=$((flags & 0xF))
  if (( live != 0 )); then ACTIVE=1; fi
  timestamp="$(date -Is)"
  temperature="$(vcgencmd measure_temp 2>/dev/null || echo 'na')"
  clock="$(vcgencmd measure_clock arm 2>/dev/null || echo 'na')"
  printf '%2d | %s | %-20s | 0x%x | %-14s | %s\n' \
    "$i" "$timestamp" "$hex" "$live" "$temperature" "$clock" | tee -a "$LOGFILE"
  if (( i < SAMPLES )); then sleep 1; fi
done

echo
echo "Decodificacao de bits:"
echo "  0x1     subtensao atual"
echo "  0x2     frequencia ARM limitada atualmente"
echo "  0x4     throttling atual"
echo "  0x8     limite de temperatura leve ativo"
echo "  0x10000 subtensao ocorrida desde a inicializacao"
echo "  0x40000 throttling ocorrido desde a inicializacao"
if (( ACTIVE )); then
  echo "ALERTA: houve pelo menos uma amostra com flags atuais ativos."
  echo "Nao comparar benchmarks de video enquanto houver subtensao/throttling."
  echo "Verifique uma fonte micro-USB 5.1V 2.5A de qualidade e o cabo."
  exit 1
else
  echo "SEM FLAGS ATUAIS nas amostras (historico pode permanecer ate reiniciar)."
fi
