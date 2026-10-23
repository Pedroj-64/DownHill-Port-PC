#!/bin/sh
# SPDX-FileCopyrightText: 2026 Pedro Soto
# SPDX-License-Identifier: GPL-3.0-or-later
# Oráculo del flujo de carrera SIN VENTANA (plan M2): por nivel exige Loading -> Countdown -> Riding y vmax >= 20 u/s.
# Con FLOW=1 además fuerza la meta (DH_FLOW_FINISH) y pulsa «repetir» (DH_FLOW_REPEAT): exige Finished -> Results -> Countdown -> Riding.
# Uso: tools/flow_check.sh [nivel...]   (por defecto ALP2 MOAB; imagen: la *.iso de la raíz o $ISO; TICKS=1800 por defecto)
set -e; cd "$(dirname "$0")/.."
ISO=${ISO:-$(ls ./*.iso 2>/dev/null | head -n 1 || true)}
[ -n "$ISO" ] && [ -x build/dhview ] || { echo "faltan la imagen .iso o build/dhview" >&2; exit 1; }
[ $# -gt 0 ] || set -- ALP2 MOAB
fail=0
for l in "$@"; do
  out=$(env SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy DH_PLAY=1 DH_LOADHOLD=0 DH_MUSIC=OFF DH_AUTOPLAY=1 ${FLOW:+DH_FLOW_FINISH=1 DH_FLOW_REPEAT=1} DH_DT=0.0166667 DH_TICKS=${TICKS:-1800} timeout 300 build/dhview "$ISO" "$l" 2>&1 || true)
  seq=$(echo "$out" | sed -n 's/^estado: .* -> \(.*\) tick=.*/\1/p' | tr '\n' ' ')
  v=$(echo "$out" | sed -n 's/.*vel máx=\([0-9]*\).*/\1/p' | head -n 1)
  want="cuenta atrás conducción "; [ -z "$FLOW" ] || want="cuenta atrás conducción meta resultados cuenta atrás conducción "
  st=OK; [ "$seq" = "$want" ] || { case "$seq" in "$want"*) ;; *) st=FLUJO;; esac; }
  [ "${v:-0}" -ge 20 ] || st="${st}_PARADA"
  echo "$l estados=[$seq] vmax=${v:-?} $st"; [ "$st" = OK ] || fail=1
done
# selector (criterio 3 del plan M2): misma lista que surf_classes sin POD*, y DH_MENU_PICK=N arranca el nivel N sin argumento de nivel
want=$(build/surf_classes "$ISO" | awk '$2 ~ /^tri=/ && $1 !~ /^POD/' | wc -l)
m=$(env SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy DH_MENU_PICK=${PICK:-0} DH_LOADHOLD=0 DH_MUSIC=OFF DH_AUTOPLAY=1 DH_DT=0.0166667 DH_TICKS=400 timeout 300 build/dhview "$ISO" 2>&1 || true)
n=$(echo "$m" | sed -n 's/^menú: \([0-9]*\) niveles.*/\1/p'); e=$(echo "$m" | sed -n 's/^estado: cuenta atrás -> conducción.*/ok/p')
st=OK; [ "$n" = "$want" ] || st=MENU; [ "$e" = ok ] || st="${st}_NOARRANCA"; echo "menú niveles=$n (esperado $want) $st"; [ "$st" = OK ] || fail=1
# volver al selector desde resultados (paso 3): meta forzada -> resultados -> relanzado -> el selector vuelve a salir (2 líneas «menú:»)
b=$(env SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy DH_MENU_PICK=${PICK:-0} DH_FLOW_FINISH=1 DH_FLOW_BACK=1 DH_LOADHOLD=0 DH_MUSIC=OFF DH_AUTOPLAY=1 DH_DT=0.0166667 DH_TICKS=1800 timeout 300 build/dhview "$ISO" 2>&1 || true)
c=$(echo "$b" | grep -c '^menú:'); st=OK; [ "$c" = 2 ] || st=VOLVER; echo "volver al selector: menús=$c (esperado 2) $st"; [ "$st" = OK ] || fail=1
# la bici y el piloto deben cargar también tras pasar por el selector (regresión del cubo rojo: variables heredadas por execv)
l2=$(env SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy DH_MENU_PICK=${PICK:-0} DH_FLOW_FINISH=1 DH_FLOW_BACK=1 DH_LOADHOLD=0 DH_MUSIC=OFF DH_AUTOPLAY=1 DH_DT=0.0166667 DH_TICKS=1800 timeout 300 build/dhview "$ISO" 2>&1 || true)
bad=$(echo "$l2" | grep -c "no válido" || true); st=OK; [ "$bad" = 0 ] || st=HEREDADO; echo "bici/piloto tras el selector: avisos=$bad $st"; [ "$st" = OK ] || fail=1
exit $fail
