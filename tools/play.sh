#!/bin/sh
# SPDX-FileCopyrightText: 2026 Pedro Soto
# SPDX-License-Identifier: GPL-3.0-or-later
# Juega un nivel a mano con un comando: tools/play.sh [NIVEL=ALP2]   (WASD/flechas, Espacio = salto, Mayús = esfuerzo; DH_INPUT_LOG=archivo guarda las entradas)
# Carga NATIVA del nivel desde unpacked/LVL/<N>.NGP (+.PTR/.TEX/.RTX y PTS/<N>.PTS): sin exportar nada con Python. Si no hay unpacked/ usa el export antiguo out/maps/<N>.mdl.
# La bici sigue siendo out/play/bike1.mdl (ensamblado en Python, tools/dh.py). Abre una ventana.
set -e; cd "$(dirname "$0")/.."; N=${1:-ALP2}
[ -f build/dhview ] || { echo "falta build/dhview (cmake --build build)" >&2; exit 1; }
[ -f out/play/bike1.mdl ] || { echo "falta out/play/bike1.mdl (python3 tools/dh.py ...)" >&2; exit 1; }
if [ -f unpacked/LVL/$N.NGP ]; then L=unpacked/LVL/$N.NGP
else
  for f in out/maps/$N.mdl out/maps/$N.col out/maps/$N.gates out/maps/$N.start.pts; do [ -f "$f" ] || { echo "falta $f (ni unpacked/LVL/$N.NGP)" >&2; exit 1; }; done
  L=out/maps/$N.mdl
fi
exec env DH_PLAY=1 DH_BIKE=out/play/bike1.mdl build/dhview "$L"
