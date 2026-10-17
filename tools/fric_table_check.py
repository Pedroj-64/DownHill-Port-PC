#!/usr/bin/env python3
"""T5 paso 2: la tabla física DAT_0077A3D8 (32 filas x 8 f32; col 0 = coef. de rozamiento de rueda, FUN_00132FD8)
vive en cada RST de nivel, en 0xCA4. Comprueba (a) que coincide byte a byte con la RAM de los volcados y
(b) que toda clase física presente en la colisión del nivel tiene col0 > 0 (54 niveles).
Uso: python3 tools/fric_table_check.py [--assert] [--rst unpacked/RST] [--ram ~/dh-states/awiiwa] [--surf build/surf_classes --iso <iso>]"""
import argparse, glob, gzip, os, re, struct, subprocess, sys

RST_OFF, TBL_RAM, TBL_LEN = 0xCA4, 0x77A3D8, 32 * 32
FX_OFF, FX_RAM, FX_LEN = 0x10D4, 0x3B7E00, 32 * 8   # tabla de efectos DAT_003b7e00: 32 registros {u32 ARGB, i16 tipo, u16 banderas}; los niveles de carrera usan clases < 32
GP = 0x2CD170  # gp - 0x7E28 = 0x2C5348 (physics-module.md); fGpffff8234 = gp - 0x7DCC = 0x2C53A4


def table(buf, off):
    return [struct.unpack_from("<8f", buf, off + 32 * c) for c in range(32)]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--assert", dest="check", action="store_true")
    ap.add_argument("--rst", default="unpacked/RST")
    ap.add_argument("--ram", default=os.path.expanduser("~/dh-states/awiiwa"))
    ap.add_argument("--surf", default="build/surf_classes")
    ap.add_argument("--iso", default="Downhill Domination.iso")
    a = ap.parse_args()
    bad = 0

    # (a) RAM == RST[0xCA4:+1024] en los volcados de carrera
    for f in sorted(glob.glob(f"{a.ram}/*_TNOS/*/snapshots/snap_001_start.ram.gz")):
        lvl = re.search(r"Z_([A-Z0-9]+)_TNOS", f)[1]
        ram = gzip.open(f).read()
        rst = open(f"{a.rst}/{lvl}.RST", "rb").read()
        ok = rst[RST_OFF:RST_OFF + TBL_LEN] == ram[TBL_RAM:TBL_RAM + TBL_LEN]
        bad += not ok
        print(f"{'OK  ' if ok else 'FALLA'} RAM == RST[0xCA4] en {lvl}")
        fx = rst[FX_OFF:FX_OFF + FX_LEN] == ram[FX_RAM:FX_RAM + FX_LEN]; bad += not fx
        print(f"{'OK  ' if fx else 'FALLA'} tabla de efectos RAM(0x3B7E00) == RST[0x10D4] (32 reg.) en {lvl}")
    g = lambda v, fmt="<f": struct.unpack_from(fmt, ram, GP + v - 0x10000)[0]
    print("gp (RAM, 1.ª carrera): ramp ini 0x8218=%.2f, ramp x 0x821C=%.2f, k 0x8220=%.2f, cap lat x 0x8234=%.2f, 0x8210=%.2f"
          % (g(0x8218), g(0x821C), g(0x8220), g(0x8234), g(0x8210)))

    # (b) 54 niveles: toda clase física presente tiene col0 > 0
    out = subprocess.run([a.surf, a.iso], capture_output=True, text=True).stdout
    n = sinrst = 0
    for line in out.splitlines():
        m = re.match(r"(\S+)\s+tri=\d+\s+física:([^|]*)\|", line)
        if not m:
            continue
        lvl, cls = m[1], [int(c.split(":")[0]) for c in m[2].split()]
        p = f"{a.rst}/{lvl}.RST"
        if not os.path.exists(p):
            print(f"AVISO {lvl}: sin RST (podio, sin conducción: hipótesis)"); sinrst += 1; continue
        t = table(open(p, "rb").read(), RST_OFF)
        miss = [c for c in cls if c < 32 and t[c][0] <= 0]
        n += 1; bad += bool(miss)
        if miss: print(f"FALLA {lvl}: clases con col0 <= 0: {miss}")
    print(f"{n} niveles con RST revisados, {sinrst} sin RST; {'PASA' if not bad else f'{bad} fallos'}")
    if a.check and bad: sys.exit(1)


main()
