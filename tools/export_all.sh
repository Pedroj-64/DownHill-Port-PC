#!/bin/sh
# SPDX-FileCopyrightText: 2026 Pedro Soto
# SPDX-License-Identifier: GPL-3.0-or-later
# Exporta todos los niveles (mdl instanciado + colisión + puertas) a out/maps/ (ignorado por git): tools/export_all.sh [paralelismo=3]
cd "$(dirname "$0")/.."; mkdir -p out/maps; P=${1:-3}
ls unpacked/LVL/*.NGP | sed 's#.*/##; s#\.NGP##' | xargs -P "$P" -I{} sh -c '
  n={}; python3 tools/extract_model.py unpacked/LVL/$n out/maps/$n.mdl > out/maps/$n.log 2>&1 \
  && python3 tools/collision.py unpacked/LVL/$n.NGP out/maps/$n.col --instances >> out/maps/$n.log 2>&1 \
  && python3 tools/markers.py unpacked/LVL/$n.NGP x out/maps/$n.start.pts out/maps/$n.gates >> out/maps/$n.log 2>&1; echo "$n $(tail -n 1 out/maps/$n.log | cut -c1-80)"'
