#!/bin/sh
# SPDX-FileCopyrightText: 2026 Pedro Soto
# SPDX-License-Identifier: GPL-3.0-or-later
# Juega un nivel a mano con un comando: tools/play.sh [NIVEL=ALP2]   (WASD/flechas, Espacio = salto, Mayús = esfuerzo; DH_INPUT_LOG=archivo guarda las entradas)
# Todo NATIVO: nivel desde unpacked/LVL/<N>.NGP (+.PTR/.TEX/.RTX y PTS/<N>.PTS) y bici ensamblada desde unpacked/BIKE; sin exportar nada con Python. Abre una ventana.
# Respaldo (sin unpacked/): export antiguo out/maps/<N>.mdl y out/play/bike1.mdl.
set -e; cd "$(dirname "$0")/.."; N=${1:-ALP2}
[ -f build/dhview ] || { echo "falta build/dhview (cmake --build build)" >&2; exit 1; }
if [ -f unpacked/LVL/$N.NGP ]; then exec env DH_PLAY=1 build/dhview unpacked/LVL/$N.NGP; fi
for f in out/maps/$N.mdl out/maps/$N.col out/maps/$N.gates out/maps/$N.start.pts out/play/bike1.mdl; do [ -f "$f" ] || { echo "falta $f (ni unpacked/LVL/$N.NGP)" >&2; exit 1; }; done
exec env DH_PLAY=1 DH_BIKE=out/play/bike1.mdl build/dhview out/maps/$N.mdl
