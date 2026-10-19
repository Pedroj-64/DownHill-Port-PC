#!/usr/bin/env python3
"""T7 paso 3: ajusta el residuo por tick (ref - fuerzas T6) contra candidatos de contacto.
Entrada: filas `R ...` de `DH_STEP1=1 DH_STEP1_K=1 DH_STEP1_ROWS=1 build/replay_demo` (una por nivel).
Uso: fric_loss_fit.py filas_NIVEL.txt [...]. Solo ticks con |v.n| < VN (rodadura).
Columnas R: surf sp vn slip resAlong resLat pedal sprint roll sprintT
/ T7 step 3: fit per-tick residual (ref - T6 forces) against contact candidates."""
import sys, numpy as np
VN = 5.0
def load(fs):
    rows = [list(map(float, l.split()[1:])) for f in fs for l in open(f) if l.startswith('R ')]
    return np.array(rows)
def fit(y, X, names, label):
    A = np.column_stack(X); c, *_ = np.linalg.lstsq(A, y, rcond=None)
    r = y - A @ c; ss0 = float(np.sum(y ** 2)); ex = 1 - float(np.sum(r ** 2)) / ss0 if ss0 else 0
    print(f"  {label}: " + " ".join(f"{n}={v:+.4g}" for n, v in zip(names, c)) + f"  explica {100*ex:.1f} % (rms {np.sqrt(np.mean(y**2)):.2f} -> {np.sqrt(np.mean(r**2)):.2f})")
    return ex
d = load(sys.argv[1:]); d = d[np.abs(d[:, 2]) < VN]
print(f"{len(d)} ticks de suelo con |v.n|<{VN}")
sp, slip, ya, yl, ped = d[:, 1], d[:, 3], d[:, 4], d[:, 5], d[:, 6]
one = np.ones_like(sp)
print("along (avance):")
e1 = fit(ya, [one], ["c"], "constante")
e2 = fit(ya, [sp], ["k*v"], "lineal -k*v (rodadura/rozamiento viscoso)")
e3 = fit(ya, [sp, sp * sp], ["a*v", "b*v2"], "v y v2 (arrastre mal calibrado)")
e4 = fit(ya, [sp, np.abs(slip)], ["k*v", "k*|slip|"], "v y |deslizamiento|")
print("lateral:")
e5 = fit(yl, [slip], ["k*slip"], "-k*slip (rozamiento lateral viscoso)")
e6 = fit(yl, [np.sign(slip)], ["F"], "Coulomb F*sign(slip)")
e7 = fit(yl, [slip, np.sign(slip)], ["k*slip", "F"], "viscoso + Coulomb")
