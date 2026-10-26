#!/bin/sh
# SPDX-FileCopyrightText: 2026 Pedro Soto
# SPDX-License-Identifier: GPL-3.0-or-later
# Juega un nivel a mano con un comando: tools/play.sh [NIVEL=ALP2] [imagen.iso]   (WASD/flechas, Espacio = salto, Mayús = esfuerzo; DH_INPUT_LOG=archivo guarda las entradas)
# Todo NATIVO, sin extraer ni exportar nada: lee la imagen ISO del jugador (la *.iso de la raíz del repo o la que indiques), descomprime los contenedores IE y construye nivel y bici en memoria. Abre una ventana.
#   Mando (SDL gamepad, src/gamepad.hpp): stick izq./cruceta = girar e inclinar, Cruz = pedalear, Triángulo = freno, R2 = salto, R1/R3 = esfuerzo, Start = reaparecer, Select = salida, Cuadrado = último punto bueno; en menús: cruceta + Cruz.
# Respaldo: carpeta extraída unpacked/ (unpacked/LVL/<N>.NGP) y, como último recurso, el export antiguo out/maps/<N>.mdl + out/play/bike1.mdl. La imagen es del jugador: no se versiona (regla 4).
set -e; cd "$(dirname "$0")/.."; N=${1:-ALP2}
[ -f build/dhview ] || { echo "falta build/dhview (cmake --build build)" >&2; exit 1; }
ISO=$2; [ -n "$ISO" ] || ISO=$(ls ./*.iso 2>/dev/null | head -n 1 || true)
if [ -n "$ISO" ] && [ -f "$ISO" ]; then exec env DH_PLAY=1 build/dhview "$ISO" "$N"; fi
if [ -f unpacked/LVL/$N.NGP ]; then exec env DH_PLAY=1 build/dhview unpacked/LVL/$N.NGP; fi
for f in out/maps/$N.mdl out/maps/$N.col out/maps/$N.gates out/maps/$N.start.pts out/play/bike1.mdl; do [ -f "$f" ] || { echo "falta $f (ni imagen .iso ni unpacked/LVL/$N.NGP)" >&2; exit 1; }; done
exec env DH_PLAY=1 DH_BIKE=out/play/bike1.mdl build/dhview out/maps/$N.mdl
