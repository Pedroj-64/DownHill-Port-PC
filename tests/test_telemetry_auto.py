# SPDX-FileCopyrightText: 2026 Pedro Soto
# SPDX-License-Identifier: GPL-3.0-or-later
"""Telemetría automática: detección de nivel/piloto, entrenador, calibración, espera de PCSX2 y modo recorrido (memoria simulada, sin PCSX2 ni datos del juego).
La prueba contra savestates reales se salta si no están (los savestates nunca van al repo)."""
import glob, json, os, sys, tempfile, types, unittest, zipfile
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'tools', 'telemetry')); sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'tools')); sys.path.insert(0, os.path.dirname(__file__))
import detect as D, record as R, schema as S
from test_telemetry import FakeLink, run_record, LEVEL_BLOCK, ROSTER_BLOCK

def sig(**kw):
    s = dict(speed=30.0, air=False, surf=1, right=False, left=False, up=False, down=False, btn=False, pedal=True, pos=(0.0, 0.0, 0.0)); s.update(kw); return s

class DetectTest(unittest.TestCase):
    def test_parse_synthetic(self):
        self.assertEqual(D.parse_level(LEVEL_BLOCK), 'ALPINE'); name, ro = D.parse_roster(ROSTER_BLOCK)
        self.assertEqual(name, 'ALPINE'); self.assertEqual([c for c, _ in ro], ['TNOS', 'CNOS', 'MNOS']); self.assertEqual(ro[0][1][0], 'GTIDRV4')
        self.assertEqual(D.parse_level(bytes(0x48)), ''); self.assertEqual(D.parse_roster(b'\xff\x00\x01' * 50), ('', []))
        self.assertEqual(D.parse_levels(b'ALPINE\0\0MOAB\0\0\0\0JUNGLE\0ALP\0MOA\0'), ['ALPINE', 'MOAB', 'JUNGLE'])      # la tabla de prefijos de 3 letras corta la lista

    def test_session_info_consistency(self):
        i = D.session_info(FakeLink(riders=3), 3); self.assertEqual((i['level'], i['player'], i['consistent']), ('ALPINE', 'TNOS', True))
        self.assertFalse(D.session_info(FakeLink(riders=3), 7)['consistent'])                  # nº de pilotos distinto: no se fía
        class Broken:
            def read_windows(self, w): raise IOError('x')
        self.assertEqual(D.session_info(Broken())['level'], '')

    def test_against_real_savestates_if_present(self):
        files = sorted(glob.glob(os.path.expanduser('~/dh-states/backup-sstates-2026-10-04/*.01.p2s')) + glob.glob(os.path.expanduser('~/dh-states/backup-sstates-2026-10-04/*.05.p2s')))
        if len(files) < 2: self.skipTest('sin savestates locales')
        import p2s
        for f, want in zip(files, [('ALPINEMX', 'MNOS', 6), ('ALPINE', 'TNOS', 10)]):
            ee = p2s.State(f).ee; link = types.SimpleNamespace(read_windows=lambda ws, ee=ee: [ee[a:a + n] for a, n in ws])
            i = D.session_info(link, p2s.State(f).u32(p2s.RIDER_COUNT_ADDR)); self.assertEqual((i['level'], i['player'], len(i['roster']), i['consistent']), (want[0], want[1], want[2], True))
            self.assertIn('ALPINE', D.level_table(link))

class CoachTest(unittest.TestCase):
    def run_seq(self, c, n, dt=0.02, **kw):
        out = []
        for _ in range(n): out += c.update(sig(**kw), dt)
        return out

    def test_items_complete(self):
        c = D.Coach(); self.assertIn('pedal', self.run_seq(c, 600)); self.assertIn('right', self.run_seq(c, 200, right=True)); self.assertIn('left', self.run_seq(c, 200, left=True))
        self.assertNotIn('pedal', self.run_seq(c, 600, right=True)); self.assertEqual(len(c.pending()), len(D.ITEMS) - 3)           # girando no cuenta como línea recta; sin repetir avisos
        self.run_seq(c, 50, btn=True); self.assertIn('jump_long', self.run_seq(c, 1, btn=False))
        self.run_seq(c, 5, btn=True); self.assertIn('jump_short', self.run_seq(c, 1, btn=False))
        self.assertIn('air', self.run_seq(c, 80, air=True)); self.assertIn('lean', self.run_seq(c, 1, up=True) + self.run_seq(c, 1, up=False) + self.run_seq(c, 1, down=True) + self.run_seq(c, 1, down=False) + self.run_seq(c, 1, up=True)
                                                                         + self.run_seq(c, 1, up=False) + self.run_seq(c, 1, down=True))

    def test_crash_brake_surfaces_time(self):
        c = D.Coach(); self.run_seq(c, 60, speed=60.0); self.assertIn('crash', self.run_seq(c, 20, speed=5.0))                      # 60 -> 5 u/s en <1 s
        c = D.Coach(); ev = []
        for _ in range(2):                                                                         # dos frenadas suaves (60 -> 31 u/s en 0,6 s) = el objetivo
            self.run_seq(c, 150, speed=60.0)
            for v in range(60, 30, -1): ev += c.update(sig(speed=float(v)), 0.02)
        self.assertIn('brake', ev); self.assertNotIn('crash', ev)                                                                   # frenada suave: no es choque
        c = D.Coach(); [self.run_seq(c, 3, surf=k) for k in (1, 2, 3)]; self.assertIn('surfaces', c.done)
        c = D.Coach(); self.assertIn('time', self.run_seq(c, 9100)); c = D.Coach(); c.update(sig(pos=(0, 0, 0)), 0.02); self.assertIn('crash', c.update(sig(pos=(500, 0, 0)), 0.02))     # reaparición = salto de posición
        self.assertEqual(c.update(sig(), 5.0), []); self.assertAlmostEqual(c.t, c.t)                                                  # una pausa larga no cuenta como tiempo de juego (dt acotado)

    def test_signals_decode_from_fake_memory(self):
        link = FakeLink(); link.tick = 5; regs = dict(zip(['ctrl', 'rider_input', 'rider_misc'], link.read_windows([(S.CTRL, 0x720), (S.RB + 0x1180, 0x1F0), (S.RB + 0x7900, 0x300)])))
        s = D.signals(regs, bytes(0x80)); self.assertAlmostEqual(s['speed'], (25.0 ** 2 + 9) ** 0.5, places=3); self.assertEqual(s['pos'], (0.0, 0.0, 0.0))

class FlowTest(unittest.TestCase):
    def test_calibrate_picks_fewer_riders_on_slow_pc(self):
        class T:
            def __init__(self, step): self.t = 0.0; self.step = step
            def __call__(self): self.t += self.step; return self.t
        class L:
            def read_windows(self, w): return [bytes(n) for _, n in w]
        r, light, ms = R.calibrate(L(), 10, now=T(0.0005)); self.assertEqual((r, light), (10, False))
        r, light, ms = R.calibrate(L(), 10, now=T(0.5)); self.assertEqual((r, light), (1, True))

    def test_connect_waits_for_pcsx2_and_game(self):
        calls = []; mod = types.ModuleType('pinelink'); fake = FakeLink()
        def mk(**kw):
            calls.append(1)
            if len(calls) < 3: raise FileNotFoundError('sin socket')
            return fake
        mod.Link = mk; old = sys.modules.get('pinelink'); sys.modules['pinelink'] = mod
        try:
            a = types.SimpleNamespace(tcp=None, socket=None, pine_slot=28011, wait=100); self.assertIs(R.connect(a, sleep=lambda s: None)[0], fake); self.assertEqual(len(calls), 3)
        finally:
            if old is not None: sys.modules['pinelink'] = old
            else: del sys.modules['pinelink']

    def test_connect_gives_up_after_wait(self):
        mod = types.ModuleType('pinelink'); mod.Link = lambda **kw: (_ for _ in ()).throw(FileNotFoundError('x')); old = sys.modules.get('pinelink'); sys.modules['pinelink'] = mod; t = iter(range(0, 10000, 400))
        try:
            with self.assertRaises(SystemExit): R.connect(types.SimpleNamespace(tcp=None, socket=None, pine_slot=1, wait=900), now=lambda: next(t), sleep=lambda s: None)
        finally:
            if old is not None: sys.modules['pinelink'] = old
            else: del sys.modules['pinelink']

    def test_race_watch_needs_two_consecutive_checks(self):
        link = FakeLink(riders=3); clock = [0.0]; w = R.RaceWatch(link, now=lambda: clock[0]); self.assertFalse(w()); clock[0] = 1.1; self.assertTrue(w())
        link.riders = 0; clock[0] = 2.3; self.assertFalse(w())                                                                       # se pierde la carrera: vuelve a pedir 2 seguidas

    def test_suggest_levels_and_riders(self):
        es, en = R.suggest([dict(level='ALPINE', rider='TNOS')], ['ALPINE', 'MOAB', 'JUNGLE', 'ALP2'], [('TNOS', []), ('CNOS', [])]); self.assertIn('MOAB', es); self.assertIn('CNOS', en); self.assertNotIn('ALPINE,', es)
        self.assertIsNone(R.suggest([dict(level='MOAB', rider='CNOS')], ['MOAB'], [('CNOS', [])]))

    def test_ctrl_c_keeps_stats(self):
        st = {}
        def boom(t):
            if t > 0.2: raise KeyboardInterrupt
        with self.assertRaises(KeyboardInterrupt): run_record(FakeLink(), 5.0, on_tick=boom, stats=st)
        self.assertGreater(st['samples'], 5)                                                                                          # las cifras sobreviven al Ctrl+C

    def test_main_tour_and_autodetected_names(self):
        for args, riders, mode, want in ((['--tour'], 0, 'tour', 'tour'), ([], 3, 'race', 'ALPINE_TNOS')):
            fake = FakeLink(riders=riders); mod = types.ModuleType('pinelink'); mod.Link = lambda **kw: fake; old = sys.modules.get('pinelink'); sys.modules['pinelink'] = mod; base = tempfile.mkdtemp()
            try: rc = R.main(['--out', base, '--seconds', '0.6', '--no-notes', '--wait', '2', '--ram-size', '0x10000', '--no-states'] + args)
            finally:
                if old is not None: sys.modules['pinelink'] = old
                else: del sys.modules['pinelink']
            zs = [f for f in os.listdir(os.path.join(base, 'SEND')) if f.endswith('.zip')]; self.assertEqual((rc, len(zs)), (0, 1)); self.assertIn(want, zs[0])
            z = zipfile.ZipFile(os.path.join(base, 'SEND', zs[0])); meta = json.loads(z.read([n for n in z.namelist() if n.endswith('meta.json')][0])); self.assertEqual(meta['mode'], mode)
            if mode == 'race':
                self.assertEqual((meta['level'], meta['rider'], meta['detected']['consistent']), ('ALPINE', 'TNOS', True)); self.assertIn('pedal', meta['coverage']); self.assertEqual(meta['riders_auto'], 10)
                self.assertTrue(os.path.exists(os.path.join(base, 'progress.json')))

class SnapshotPolicyTest(unittest.TestCase):
    def test_periodic_snapshot_uses_state_only_but_start_keeps_ram(self):
        import snapshots as SN
        sd = tempfile.mkdtemp(); link = FakeLink(); link.sdir = sd; sv = SN.StateSaver(link, 'SLES-52202', sd, 10); out = tempfile.mkdtemp(); log = []
        sn = R.Snapper(link, out, every=0, states=sv, log=log, ram_size=0x10000)
        sn.take('start'); sn.take('auto'); sn.take('cambio_JUNGLE'); sn.take('resultados')
        ext = [sorted(f.rsplit('.', 2)[-2] + '.' + f.rsplit('.', 1)[-1] if f.endswith('.gz') else f.rsplit('.', 1)[-1] for f in r['files']) for r in log]
        self.assertEqual([len(r['files']) for r in log], [2, 1, 1, 2]); self.assertTrue(log[1]['files'][0].endswith('.p2s'))

class AutoLoopTest(unittest.TestCase):
    def test_menus_race_menus_without_any_flag(self):
        import time
        class Script(FakeLink):                 # 0-1 s menús, 1-4 s carrera, luego menús; a los 9,5 s el usuario pulsa Ctrl+C
            def __init__(self): super().__init__(riders=0); self.t0 = time.monotonic()
            def _read(self, a, ln):
                t = time.monotonic() - self.t0; self.riders = 3 if 1.0 <= t < 4.0 else 0
                return super()._read(a, ln)
            def status(self):
                if time.monotonic() - self.t0 > 9.5: raise KeyboardInterrupt
                return 0
        fake = Script(); mod = types.ModuleType('pinelink'); mod.Link = lambda **kw: fake; old = sys.modules.get('pinelink'); sys.modules['pinelink'] = mod; base = tempfile.mkdtemp()
        try: rc = R.main(['--out', base, '--no-notes', '--no-states', '--ram-size', '0x10000', '--wait', '5'])
        finally:
            if old is not None: sys.modules['pinelink'] = old
            else: del sys.modules['pinelink']
        zs = sorted(f for f in os.listdir(os.path.join(base, 'SEND')) if f.endswith('.zip')); self.assertEqual(rc, 0)
        modes = [json.loads(zipfile.ZipFile(os.path.join(base, 'SEND', z)).read([n for n in zipfile.ZipFile(os.path.join(base, 'SEND', z)).namelist() if n.endswith('meta.json')][0]))['mode'] for z in zs]
        self.assertIn('race', modes); self.assertEqual(modes.count('race'), 1)                              # la carrera se detectó y grabó sola
        self.assertTrue(any('ALPINE_TNOS' in z for z in zs))                                                # nivel y piloto sin escribir nada

if __name__ == '__main__': unittest.main()
