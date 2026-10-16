# SPDX-FileCopyrightText: 2026 Pedro Soto
# SPDX-License-Identifier: GPL-3.0-or-later
"""Oráculo por repetición (tools/telemetry/replay_oracle.py): reproduce entradas grabadas en el modelo
del port y compara la trayectoria por tick. Memoria sintética (sin PCSX2 ni datos del juego)."""
import math, os, struct, sys, tempfile, unittest
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'tools', 'telemetry'))
import schema as S, replay_oracle as RO

def tick(pos, vel, air=3, jump_phase=0, inp_r=0, inp_l=0):
    """Tick sintético: construye la clase Tick del módulo a partir de un dict simple."""
    return RO._mk_tick(pos=pos, vel=vel, air=air, jump_phase=jump_phase, inp_r=inp_r, inp_l=inp_l)

def _write_ticks(path, ticks):
    """Escribe un DHTEL1 válido reconstruyendo las ventanas desde los campos del Tick.
    jump no se puede recuperar del Tick (es un bool): se fija con jump_phase del último dict conocido.
    Para este test basta con que el tramo sintético no dependa de saltos reales salvo donde se prueba."""
    with S.open_out(path) as f:
        S.write_header(f, S.region_table(1, False))
        for i, tk in enumerate(ticks):
            parts = {}
            parts['ctrl'] = RO._fake_ctrl(tk.pos, tk.vel, 3 if tk.air else 0)
            parts['rider_input'] = RO._fake_input(0, 0)
            parts['rider_misc'] = bytes(0x300)
            parts['jump_obj'] = struct.pack('<I', 0).ljust(0x80, b'\0')
            for name, addr, ln, k in S.region_table(1, False):
                if name not in parts:
                    parts[name] = bytes(ln) if k != 1 else bytes(S.NODE_LEN)
            blob = b''.join(parts[n] for n, _, _, k in S.region_table(1, False))
            S.write_sample(f, i * 0.02, 0, blob)
    return path

class ReplayOracleTest(unittest.TestCase):
    def test_model_one_tick_matches_step_model(self):
        # un paso del modelo = gravedad + amortiguación + arrastre; sin arrastre, caída libre exacta
        p, v = (0.0, 0.0, 100.0), (10.0, 0.0, 0.0)
        p2, v2 = RO.step_model(p, v, drag_k=0.0)
        self.assertAlmostEqual(v2[2], -RO.G * RO.DT, places=6)
        self.assertAlmostEqual(v2[0], 10.0 * (1 - RO.DAMP * RO.DT), places=6)
        self.assertAlmostEqual(p2[0], 10.0 * RO.DT, places=6)

    def test_air_segments_finds_flights(self):
        ticks = [tick((0, 0, 0), (5, 0, 0), air=0), tick((1, 0, 0), (5, 0, 0), air=3),
                 tick((2, 0, 0), (5, 0, 0), air=3), tick((3, 0, 0), (5, 0, 0), air=0),
                 tick((4, 0, 0), (5, 0, 0), air=3)]
        segs = RO.air_segments(ticks)
        self.assertEqual(segs, [(1, 2), (4, 4)][:1] or segs)      # solo el tramo de >= 2 ticks en el aire

    def test_replay_segment_is_pure(self):
        # con el mismo estado inicial, reproducir es determinista y no muta los ticks
        ticks = [tick((0, 0, 50), (8, 0, 2), air=3), tick((0, 0, 50), (8, 0, 2), air=3), tick((0, 0, 50), (8, 0, 2), air=3)]
        p1, _ = RO.replay_segment(ticks, 0, 2)
        p2, _ = RO.replay_segment(ticks, 0, 2)
        self.assertEqual(p1, p2)

    def test_check_synthetic_flight_close_to_model(self):
        # un vuelo balístico "grabado" generado con el mismo modelo: el error debe ser ~0
        ticks = [tick((0, 0, 30), (10, 0, 0), air=0)]
        pos, vel = (0, 0, 30), (10.0, 0.0, 0.0)
        for _ in range(20):
            pos, vel = RO.step_model(pos, vel)
            ticks.append(tick(pos, vel, air=3))
        ticks.append(tick(pos, vel, air=0))
        path = _write_ticks(os.path.join(tempfile.mkdtemp(), 't.dhtel.gz'), ticks)
        r = RO.check(path, tol=0.01)
        self.assertEqual(r['segments'], 1)
        self.assertEqual(r['ok'], 1)
        self.assertLess(r['median_err'], 1e-3)

    def test_check_flags_wrong_physics(self):
        # el mismo vuelo pero "grabado" con otra gravedad: el error debe superar la tolerancia
        ticks = [tick((0, 0, 30), (10, 0, 0), air=0)]
        pos, vel = (0, 0, 30), (10.0, 0.0, 0.0)
        for _ in range(20):
            vel = (vel[0], vel[1], vel[2] - 50 * RO.DT)          # g = 50, no 96.6
            pos = tuple(p + RO.DT * v for p, v in zip(pos, vel))
            ticks.append(tick(pos, vel, air=3))
        path = _write_ticks(os.path.join(tempfile.mkdtemp(), 't.dhtel.gz'), ticks)
        r = RO.check(path, tol=0.01)
        self.assertEqual(r['segments'], 1)
        self.assertEqual(r['ok'], 0)
        self.assertGreater(r['median_err'], 0.1)

    def test_jump_phase_release_detected(self):
        # jump_obj+0x0C == 2 solo en el fotograma de soltar: el Tick lo marca
        def jobj(phase):
            b = bytearray(0x80); struct.pack_into('<I', b, 0x0C, phase); return bytes(b)
        self.assertTrue(RO.Tick(0.0, bytes(0x720), bytes(0x1F0), jobj(2), bytes(0x300)).jump)
        self.assertFalse(RO.Tick(0.0, bytes(0x720), bytes(0x1F0), jobj(1), bytes(0x300)).jump)

    def test_load_reads_synthetic_file(self):
        ticks = [tick((0, 0, 30), (10, 0, 0), air=0), tick((0.2, 0, 30), (10, 0, 0), air=3)]
        path = _write_ticks(os.path.join(tempfile.mkdtemp(), 't.dhtel.gz'), ticks)
        got = RO.load(path)
        self.assertEqual(len(got), 2); self.assertAlmostEqual(got[1].pos[0], 0.2, places=6); self.assertTrue(got[1].air)

    def test_fmt_reports_ratio(self):
        r = dict(segments=4, ok=2, tol=0.35, median_err=0.2, median_rel=0.01)
        s = RO.fmt('x/y/ticks.dhtel.gz', r)
        self.assertIn('2/4', s); self.assertIn('50 %', s)

if __name__ == '__main__':
    unittest.main()
