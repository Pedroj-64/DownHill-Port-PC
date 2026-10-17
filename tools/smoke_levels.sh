#!/bin/sh
# SPDX-FileCopyrightText: 2026 Pedro Soto
# SPDX-License-Identifier: GPL-3.0-or-later
# Prueba de humo de movimiento SIN VENTANA en todos los niveles: carga cada nivel de la ISO, lo recorre con el piloto automático
# (DH_AUTOSTEER/DH_AUTOPEDAL, determinista con DH_DT fijo) y tabula puertas, reinicios y velocidad máxima. Falla si un nivel no
# produce resumen, trae NaN o no cruza ninguna puerta.
# Uso: tools/smoke_levels.sh [imagen.iso] [ticks=3600] [nivel...]   (JOBS=1 por defecto: dhview con render por software llega a varios GB y con más el kernel mata procesos por falta de memoria (OOM); un reintento por nivel)
set -e; cd "$(dirname "$0")/.."
if [ "$1" = "--one" ]; then   # trabajador: --one ISO TICKS NIVEL
  ISO=$2; TICKS=$3; l=$4; tmp=$(mktemp)
  for try in 1 2; do
    env SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy DH_PLAY=1 DH_LOADHOLD=0 DH_MUSIC=OFF DH_SFX=1 DH_AUTOPLAY=1 \
        DH_DT=0.0166667 DH_TICKS="$TICKS" timeout 300 build/dhview "$ISO" "$l" >"$tmp" 2>&1 || true
    line=$(grep -o 'sfx resumen:.*' "$tmp" | head -n 1); [ -n "$line" ] && break
  done; rm -f "$tmp"
  [ -n "$line" ] || { echo "$l SIN_RESUMEN"; exit 0; }
  g=$(echo "$line" | sed -n 's/.*puertas=\([0-9]*\/[0-9]*\).*/\1/p'); r=$(echo "$line" | sed -n 's/.*reinicios=\([0-9]*\).*/\1/p')
  v=$(echo "$line" | sed -n 's/.*vel máx=\([^ ]*\).*/\1/p'); st=OK
  case "$line" in *[nN]a[nN]*|*inf*) st=NAN;; esac
  [ "${g%%/*}" = 0 ] && [ "${g##*/}" != 0 ] && st="${st}_SIN_PUERTAS"   # solo exige puertas en niveles que las tienen (MONKEY/TRAINER/MOSH: 0/0)
  [ "${v%%.*}" -lt 20 ] 2>/dev/null && st="${st}_PARADA"                   # la moto no llegó a 20 u/s: atascada en la salida
  echo "$l puertas=$g reinicios=$r vmax=$v $st"; exit 0
fi
ISO=$1; [ -n "$ISO" ] || ISO=$(ls ./*.iso 2>/dev/null | head -n 1 || true)
[ -n "$ISO" ] || { echo "falta la imagen .iso" >&2; exit 1; }
[ -x build/dhview ] && [ -x build/surf_classes ] || { echo "faltan build/dhview o build/surf_classes (cmake --build build)" >&2; exit 1; }
TICKS=${2:-3600}
if [ $# -gt 2 ]; then shift 2; LEVELS="$*"; else LEVELS=$(build/surf_classes "$ISO" | awk '$2 ~ /^tri=/ && $1 !~ /^POD/ {print $1}'); fi   # sin POD*: escenas de podio sin parrilla de salida ni línea (no se corren; hipótesis)
out=$(printf '%s\n' $LEVELS | xargs -P "${JOBS:-1}" -I{} sh "$0" --one "$ISO" "$TICKS" {} | sort)
echo "$out"
bad=$(echo "$out" | grep -vc ' OK$' || true)
n=$(echo "$out" | wc -l)
echo "humo: $n niveles, $bad con fallo"
[ "$bad" -eq 0 ]
