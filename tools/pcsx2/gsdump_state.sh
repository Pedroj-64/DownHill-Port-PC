#!/bin/sh
# SPDX-FileCopyrightText: 2026 Pedro Soto
# SPDX-License-Identifier: GPL-3.0-or-later
# Carga la ranura N del PCSX2 anidado (nested.sh) e inmediatamente vuelca un fotograma del GS (Shift+F8). Copia el .gs.zst y el .png a ~/dh-states/gs/<etiqueta>.*
# Uso: gsdump_state.sh <ranura 1-10> <etiqueta> [ISO]    (arranca el anidado si hace falta; lo deja encendido)
# Las teclas solo van al compositor anidado (sendkey.sh). Se verifica el efecto: el archivo debe aparecer en snaps/ o se aborta.
set -eu
here=$(dirname "$0"); slot=${1:?ranura}; tag=${2:?etiqueta}
iso=${3:-$(cd "$here/../.." && pwd)/Downhill Domination.iso}
snaps="$HOME/.var/app/net.pcsx2.PCSX2/config/PCSX2/snaps"; out="$HOME/dh-states/gs"; mkdir -p "$out"
sh "$here/nested.sh" status | grep -q activo || { sh "$here/nested.sh" start "$iso"; sleep 30; }
# La ranura actual del emulador se desconoce: se carga una vez (F3), se lee del registro "Estado cargado de la ranura N" y se avanza con F2 hasta la pedida.
log="$HOME/.var/app/net.pcsx2.PCSX2/config/PCSX2/logs/emulog.txt"
cur_slot() { sh "$here/sendkey.sh" F3 >/dev/null; sleep 3; grep -o 'Estado cargado de la ranura [0-9]*' "$log" | tail -1 | grep -o '[0-9]*$'; }
cur=$(cur_slot); [ -n "$cur" ] || { echo "no se pudo leer la ranura actual (F3 no llegó)" >&2; exit 1; }
k=$(( (slot - cur + 10) % 10 )); i=0; while [ $i -lt $k ]; do sh "$here/sendkey.sh" F2 >/dev/null; i=$((i+1)); done
touch "$out/.mark"
sh "$here/sendkey.sh" F3 >/dev/null; sleep "${DH_AFTER_LOAD:-2}"
sh "$here/sendkey.sh" -m shift F8 >/dev/null
n=0; while [ $n -lt 40 ]; do f=$(ls -t "$snaps"/*.gs.zst 2>/dev/null | head -1 || true); [ -n "$f" ] && [ "$f" -nt "$out/.mark" ] && break; sleep 1; n=$((n+1)); done
[ -n "${f:-}" ] && [ "$f" -nt "$out/.mark" ] || { echo "no apareció el volcado GS (la tecla no llegó)" >&2; exit 1; }
sleep 2; cp "$f" "$out/$tag.gs.zst"; cp "${f%.gs.zst}.png" "$out/$tag.png" 2>/dev/null || true
echo "ok: $out/$tag.gs.zst"
