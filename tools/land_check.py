#!/usr/bin/env python3
"""Oráculo de aterrizaje T5: |v| del port y de la referencia a +0,5 s y +1,5 s del toque (ALPINE t=114,4).
Uso: python3 tools/land_check.py [--assert]   (necesita build/replay_demo, out/maps/ALPINE.col y ~/dh-states/replay/ALPINE.replay)"""
import math, os, re, subprocess, sys
DT = 0.02  # paso de tests/replay_demo.cpp
home = os.path.expanduser("~")
env = dict(os.environ, DH_LAND="1", DH_TRACEWIN="114.4", DH_TRACESTRIDE="1")
out = subprocess.run(["build/replay_demo", "out/maps/ALPINE.col", f"{home}/dh-states/replay/ALPINE.replay", "2", "0.5"],
                     env=env, capture_output=True, text=True, timeout=300).stdout
rx = re.compile(r"s=(\d+) port .*? v=\(([-\d.]+) ([-\d.]+) ([-\d.]+)\) gr=\d \| ref .*? v=\(([-\d.]+) ([-\d.]+) ([-\d.]+)\) air=(\d)")
rows = {int(m[1]): (math.dist((0,0,0), map(float, m.group(2, 3, 4))), math.dist((0,0,0), map(float, m.group(5, 6, 7))), int(m[8]))
        for m in map(rx.search, out.splitlines()) if m}
touch = next((s for s in sorted(rows) if rows[s][2] == 0 and s > 1 and rows[s - 1][2] == 1), None)
if touch is None: sys.exit("no se encontró el toque en la traza")
res = {}
for lab, t, lo, hi in (("+0,5 s", 0.5, 26, 31), ("+1,5 s", 1.5, 38, 45)):
    s = touch + round(t / DT)
    if s not in rows: sys.exit(f"la traza no llega a {lab}")
    p, r, _ = rows[s]; ok = lo <= p <= hi; res[lab] = ok
    print(f"{lab}: port {p:.1f} ref {r:.1f} u/s (criterio propuesto [{lo}, {hi}]) {'OK' if ok else 'FUERA'}")
if "--assert" in sys.argv and not all(res.values()): sys.exit(1)
