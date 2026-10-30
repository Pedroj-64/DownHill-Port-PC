#!/bin/sh
# SPDX-FileCopyrightText: 2026 Pedro Soto
# SPDX-License-Identifier: GPL-3.0-or-later
# Comprueba SIN VENTANA que los efectos de sonido se disparan al rodar: bucle de rodadura, viento en el aire,
# aterrizaje, choque/reinicio y puerta. Determinista (DH_DT fijo de 1 tick/fotograma y DH_TICKS), sin teclado ni ratón.
# Uso: tools/sfx_check.sh [imagen.iso] [NIVEL=JUNGLE] [ticks=20000]
# JUNGLE sin mando (cuesta abajo) rueda, salta, aterriza, cruza puertas y se reinicia una vez; con otro nivel/mando
# puede que algún evento no salga, así que los cinco contadores deben ser > 0 con los valores por defecto.
set -e; cd "$(dirname "$0")/.."
ISO=$1; [ -n "$ISO" ] || ISO=$(ls ./*.iso 2>/dev/null | head -n 1 || true)
[ -n "$ISO" ] || { echo "falta la imagen .iso (raíz del repo o primer argumento)" >&2; exit 1; }
[ -x build/dhview ] || { echo "falta build/dhview (cmake --build build)" >&2; exit 1; }
LEVEL=${2:-JUNGLE}; TICKS=${3:-20000}
tmp=$(mktemp); trap 'rm -f "$tmp"' EXIT
env SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy DH_PLAY=1 DH_LOADHOLD=0 DH_MUSIC=OFF DH_SFX=1 DH_NOBLOCKREV=1 \
    DH_DT=0.0166667 DH_TICKS="$TICKS" DH_PLAYIN="0 0 0 0" timeout 240 build/dhview "$ISO" "$LEVEL" 2>"$tmp" >/dev/null
line=$(grep -o 'sfx resumen:.*' "$tmp" | head -n 1)
[ -n "$line" ] || { echo "no se obtuvo el resumen de efectos (¿DH_TICKS demasiado alto o banco ausente?)" >&2; cat "$tmp" >&2; exit 1; }
echo "$line"
val() { printf '%s\n' "$line" | sed -n "s/.*[^a-zá-ú]$1=\([0-9][0-9]*\).*/\1/p"; }
roll=$(val rodadura); air=$(val aire); chk=$(val checkpoint); land=$(val aterrizaje); crash=$(val choque)
for pair in "rodadura:$roll" "viento:$air" "puerta:$chk" "aterrizaje:$land" "choque:$crash"; do
    name=${pair%%:*}; v=${pair#*:}
    if [ -z "$v" ] || [ "$v" -le 0 ]; then echo "FALLO: el evento '$name' no se disparó (valor '$v')" >&2; exit 1; fi
done
echo "sfx_check OK (rodadura=$roll viento=$air puerta=$chk aterrizaje=$land choque=$crash)"
