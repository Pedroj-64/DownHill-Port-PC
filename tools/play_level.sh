#!/bin/sh
# SPDX-FileCopyrightText: 2026 Pedro Soto
# SPDX-License-Identifier: GPL-3.0-or-later
# Prepara y lanza un nivel con la colisión del juego: tools/play_level.sh ALP2 [frames]   (necesita unpacked/ y build/dhview; escribe en out/play/, ignorado por git)
# Genera: <N>.mdl (+ .sky.mdl), <N>.col (colisión), <N>.gates (puertas), chain0.pts (línea de carrera desde el registro 0)
set -e; cd "$(dirname "$0")/.."; N=${1:-ALP2}; mkdir -p out/play
python3 tools/extract_model.py unpacked/LVL/$N out/play/$N.mdl
python3 tools/collision.py unpacked/LVL/$N.NGP out/play/$N.col
python3 tools/markers.py unpacked/LVL/$N.NGP x out/play/start.pts out/play/$N.gates
python3 tools/pts_path.py unpacked/PTS/$N.PTS out/play/chain0.pts --chain 0
[ -n "$2" ] && export DH_FRAMES=$2 DH_DT=0.016667
exec env DH_RIDE=1 DH_PTS=out/play/chain0.pts build/dhview out/play/$N.mdl
