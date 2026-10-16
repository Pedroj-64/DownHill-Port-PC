#!/bin/sh
# SPDX-FileCopyrightText: 2026 Pedro Soto
# SPDX-License-Identifier: GPL-3.0-or-later
# Juega un nivel a mano con un comando: tools/play.sh [NIVEL=ALP2]   (WASD/flechas, Espacio = salto, Mayús = esfuerzo; DH_INPUT_LOG=archivo guarda las entradas)
# Necesita los niveles exportados (out/maps/<N>.mdl|.col|.gates|.start.pts, `python3 tools/dh.py doctor`), la bici (out/play/bike1.mdl) y build/dhview. Abre una ventana.
set -e; cd "$(dirname "$0")/.."; N=${1:-ALP2}
for f in build/dhview out/maps/$N.mdl out/maps/$N.col out/maps/$N.gates out/maps/$N.start.pts out/play/bike1.mdl; do
  [ -f "$f" ] || { echo "falta $f (compila con cmake --build build y exporta con tools/export_all.sh / dh.py doctor)" >&2; exit 1; }
done
exec env DH_PLAY=1 DH_BIKE=out/play/bike1.mdl build/dhview out/maps/$N.mdl
