# SPDX-FileCopyrightText: 2026 Pedro Soto
# SPDX-License-Identifier: GPL-3.0-or-later
"""Telemetría (tools/telemetry): formato, grabación contra una memoria simulada (sin PCSX2 ni datos del juego), comprobación, empaquetado y salvaguardas."""
import gzip, os, struct, sys, tempfile, types, unittest, zipfile
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'tools', 'telemetry'))
import schema as S, record as R, check as C, bundle as B, snapshots as SN, detect as D

def _pad(b, n): return b + bytes(n - len(b))
LEVEL_BLOCK = _pad(bytes(4) + b'ALPINE\0'.ljust(9, b'\0') + b'\\LVL\\ALPINE.NGP;1\0', D.LEVEL_WIN[1])
ROSTER_BLOCK = _pad(b'ALPINE'.ljust(10, b'\0') + b''.join(c.ljust(6, b'\0') + b'GTIDRV4\0\0\0\0ROXPS100\0\0ENTRWH\0\0\0\0BIKESKEL\0\0' for c in (b'TNOS', b'CNOS', b'MNOS')), D.ROSTER_WIN[1])
LEVELS_BLOCK = _pad(b''.join(n.ljust(8, b'\0') for n in (b'ALPINE', b'MOAB', b'JUNGLE', b'ALP2', b'ALP', b'MOA')), D.LEVELS_WIN[1])

class FakeLink:
    """Memoria simulada: el 'juego' avanza un paso en cada lectura de la ventana SYNC impar (la de antes de la muestra)."""
    def __init__(self, riders=3, game='SLES-52202', frozen=False, paused=False):
        self.tick = 0; self.syncs = 0; self.riders = riders; self.game = game; self.frozen = frozen; self.paused = paused; self.where = 'fake'
    def game_id(self): return self.game
    def title(self): return 'Downhill Domination'
    def version(self): return 'fake-pine'
    def status(self): return 1 if self.paused else 0
    def save_state(self, slot):
        if getattr(self, 'sdir', None): open(os.path.join(self.sdir, f'SLES-52202 (DEADBEEF).{slot:02d}.p2s'), 'wb').write(b'STATE' * 1000 + bytes([self.tick % 250]))
    def _read(self, a, ln):
        b = bytearray(ln)
        def put(addr, data):
            if a <= addr and addr + len(data) <= a + ln: b[addr - a:addr - a + len(data)] = data
        put(S.RIDER_COUNT_ADDR, struct.pack('<I', self.riders))
        for i in range(self.riders): put(S.rb(i) + 0x7928 + 4, struct.pack('<I', 0x500000 + 0x1000 * i))
        put(D.LEVEL_WIN[0], LEVEL_BLOCK); put(D.ROSTER_WIN[0], ROSTER_BLOCK); put(D.LEVELS_WIN[0], LEVELS_BLOCK)
        t = self.tick
        # SYNC window (ctrl+0x50, 0x40): posición y momento que cambian cada paso
        put(S.CTRL + 0x50, struct.pack('<16f', *[t * 0.5 + k for k in range(16)]))
        put(S.CTRL + 0xC0, struct.pack('<3f', 20.0 + (t % 10), 0.0, -3.0)); put(S.CTRL + 0x4A4, struct.pack('<h', 0 if t % 40 < 30 else 3)); put(S.CTRL + 0x396, struct.pack('<H', 4))
        put(S.RB + 0x1190, struct.pack('<i', 1 if t % 20 < 10 else 0)); put(S.RB + 0x7900 + 0x6A, bytes([1 if t % 50 < 25 else 0]))
        return bytes(b)
    def read_windows(self, windows):
        if len(windows) == 1 and windows[0] == S.SYNC:
            self.syncs += 1
            if self.syncs % 2 == 1 and not self.frozen: self.tick += 1
        return [self._read(a, ln) for a, ln in windows]

def run_record(link, seconds=0.4, riders=3, **kw):
    path = os.path.join(tempfile.mkdtemp(), 'ticks.dhtel.gz')
    with S.open_out(path) as f:
        S.write_header(f, S.region_table(riders)); st = R.record(link, f, seconds, riders, **kw)
    return path, st

class TelemetryTest(unittest.TestCase):
    def test_roundtrip_plain_and_gzip(self):
        t = S.region_table(2, slow=False); blob = bytes(range(256)) * (S.sample_len(t) // 256) + bytes(S.sample_len(t) % 256)
        for name in ('a.dhtel', 'a.dhtel.gz'):
            p = os.path.join(tempfile.mkdtemp(), name)
            with S.open_out(p) as f: S.write_header(f, t); S.write_sample(f, 1.5, 1, blob); S.write_sample(f, 2.0, 0, blob)
            tab, smp = S.read_file(p); self.assertEqual(len(smp), 2); self.assertEqual(smp[0][1], 1); self.assertEqual(S.sample_len(tab), len(blob)); self.assertEqual(len(smp[1][2]['ctrl']), 0x720)
        for l in (8, 0x1F0, 0x720, 0x1E0):                                   # todas las ventanas fijas son múltiplos de 8 (Read64)
            self.assertEqual(l % 8, 0)
        for n, a, l, k in S.region_table(10): self.assertTrue(k == 1 or (a % 8 == 0 and l % 8 == 0), n)

    def test_rejects_garbage_and_salvages_truncated_gzip(self):
        p = os.path.join(tempfile.mkdtemp(), 'x.dhtel'); open(p, 'wb').write(b'nada'); self.assertRaises(ValueError, S.read_file, p)
        path, st = run_record(FakeLink(), 0.3); raw = open(path, 'rb').read(); cut = os.path.join(os.path.dirname(path), 'cut.dhtel.gz'); open(cut, 'wb').write(raw[:len(raw) * 2 // 3])
        _, smp = S.read_file(cut); self.assertGreater(len(smp), 0); self.assertLess(len(smp), st['samples'])        # un archivo cortado (cierre brusco) aún da muestras

    def test_record_samples_and_check_ok(self):
        path, st = run_record(FakeLink(), 0.5); tab, smp = S.read_file(path); self.assertGreater(st['samples'], 20); self.assertEqual(len(smp), st['samples']); self.assertEqual(st['forced'], 0)
        self.assertIn('r2_ctrl', [n for n, _, _, _ in tab]); self.assertIn('r2_node', [n for n, _, _, _ in tab])
        folder = os.path.dirname(path); r = C.analyse(folder); self.assertGreater(r['inputs']['derecha_right'], 0); self.assertGreater(r['inputs']['pedal_7A6A'], 0); self.assertGreater(r['air_fraction'], 0)
        self.assertIn('r1', r['others']); self.assertTrue(any('corta' in p or 'short' in p for p in r['problems']))      # 0.5 s es muy corta: lo avisa

    def test_frozen_game_gives_forced_samples_and_pause_gives_none(self):
        _, st = run_record(FakeLink(frozen=True), 0.4); self.assertGreater(st['forced'], 3); self.assertEqual(st['forced'], st['samples'] - 1)     # la primera muestra siempre es 'nueva'
        _, st = run_record(FakeLink(paused=True), 0.2); self.assertEqual(st['samples'], 0)

    def test_race_end_stops_recording(self):
        _, st = run_record(FakeLink(riders=0), 5.0, lost_after=0.2); self.assertEqual(st['ended'], 'race-ended'); self.assertEqual(st['samples'], 0)

    def test_game_check_and_repo_guard(self):
        with self.assertRaises(SystemExit): R.check_game(FakeLink(game='SLUS-00000'))
        self.assertEqual(R.check_game(FakeLink(game='SLUS-00000'), force=True)[0], 'SLUS-00000'); self.assertEqual(R.check_game(FakeLink())[0], 'SLES-52202')
        self.assertTrue(R.in_git_repo(os.path.dirname(__file__))); self.assertFalse(R.in_git_repo(tempfile.mkdtemp()))

    def test_main_end_to_end_and_bundle(self):
        fake = FakeLink(); mod = types.ModuleType('pinelink'); mod.Link = lambda **kw: fake; old = sys.modules.get('pinelink'); sys.modules['pinelink'] = mod
        base = tempfile.mkdtemp()
        try: rc = R.main(['--out', base, '--level', 'ALP2', '--rider', 'Test', '--seconds', '0.5', '--no-notes', '--wait', '2', '--ram-size', '0x100000', '--no-states'])
        finally:
            if old is not None: sys.modules['pinelink'] = old
            else: del sys.modules['pinelink']
        self.assertEqual(rc, 0); send = os.path.join(base, 'SEND'); zs = [f for f in os.listdir(send) if f.endswith('.zip')]; self.assertEqual(len(zs), 1); self.assertTrue(os.path.exists(os.path.join(send, 'LEEME_PRIMERO_README_FIRST.txt')))
        names = zipfile.ZipFile(os.path.join(send, zs[0])).namelist(); [self.assertTrue(any(n.endswith(x) for n in names), x) for x in ('ticks.dhtel.gz', 'static.dhtel', 'static_end.dhtel', 'meta.json', 'notes.jsonl', 'snap_001_start.ram.gz', 'snap_002_end.ram.gz')]
        self.assertIn('ALP2_Test', zs[0]); self.assertEqual(C.main([os.path.join(send, zs[0])]), 1)             # válida pero demasiado corta -> pide repetir
        self.assertEqual(B.main(['--out', base]), 0); self.assertTrue([f for f in os.listdir(base) if f.startswith('dhtel_bundle_')])
        tab, smp = S.read_file(os.path.join(base, 'sessions', zs[0][:-4], 'ticks.dhtel.gz')); st_tab, st_smp = S.read_file(os.path.join(base, 'sessions', zs[0][:-4], 'static.dhtel'))
        self.assertEqual(len(st_smp), 1); self.assertIn('rider0_full', st_smp[0][2]); self.assertEqual(len(st_smp[0][2]['rider0_full']), S.RIDER_STRIDE)

    def test_slow_channel_present_and_complete(self):
        path, st = run_record(FakeLink(riders=3), 0.5); tab, smp = S.read_file(path); slow = [p for _, fl, p in smp if fl & 2]
        self.assertGreater(len(slow), 0); self.assertEqual(len(slow), st['slow']); self.assertEqual(len(slow[0]['full_r2']), S.RIDER_STRIDE); self.assertEqual(len(slow[0]['race_mgr_wide']), 0x1000)
        self.assertTrue(all('full_r0' not in p for _, fl, p in smp if not fl & 2)); self.assertEqual(smp[0][1] & 2, 2)           # la primera muestra trae el canal lento
        light = os.path.join(tempfile.mkdtemp(), 't.gz')
        with S.open_out(light) as f: S.write_header(f, S.region_table(1, False)); R.record(FakeLink(), f, 0.2, 1, slow=False)
        self.assertEqual(S.read_file(light)[0].slow_period, 0)

    def test_tour_mode_records_without_a_race(self):
        path = os.path.join(tempfile.mkdtemp(), 't.gz')
        with S.open_out(path) as f: S.write_header(f, S.region_table(1)); st = R.record(FakeLink(riders=0), f, 0.5, 1, require_race=False, min_interval=0.05)
        self.assertGreater(st['samples'], 3); self.assertEqual(S.read_file(path)[1][0][2]['rider_count'], bytes(8))

    def test_ram_dump_and_snapper_with_snap_command(self):
        d = tempfile.mkdtemp(); p = os.path.join(d, 'x.ram.gz'); self.assertEqual(SN.ram_dump(FakeLink(), p, 0x40000), 0x40000); self.assertEqual(len(gzip.open(p).read()), 0x40000)
        notes = R.Notes(False); notes.add('snap menu principal'); notes.add('derrape fuerte'); log = []; sn = R.Snapper(FakeLink(), d, every=0, notes=notes, log=log, ram_size=0x10000); sn.last_race = True; sn.tick(1.0)
        self.assertEqual(len(log), 1); self.assertEqual(log[0]['label'], 'menu principal'); self.assertTrue(os.path.exists(os.path.join(d, 'snapshots', log[0]['files'][0]))); self.assertEqual(len(notes.items), 2)
        for i in range(5): sn.max = 2; sn.take('x')
        self.assertLessEqual(len(log), 2)                                                                            # límite de fotos

    def test_state_saver_backs_up_and_restores_user_slot(self):
        sd = tempfile.mkdtemp(); mine = os.path.join(sd, 'SLES-52202 (DEADBEEF).10.p2s'); open(mine, 'wb').write(b'MI_SAVESTATE')
        link = FakeLink(); link.sdir = sd; sv = SN.StateSaver(link, 'SLES-52202', sd, 10); self.assertTrue(os.path.exists(mine + '.dhtel_backup'))
        dest = os.path.join(tempfile.mkdtemp(), 'snap.p2s'); self.assertGreater(sv.save(dest, timeout=15), 1000); self.assertTrue(os.path.getsize(dest) > 1000)
        sv.restore(); self.assertEqual(open(mine, 'rb').read(), b'MI_SAVESTATE'); self.assertFalse(os.path.exists(mine + '.dhtel_backup'))                 # tu savestate vuelve intacto
        sd2 = tempfile.mkdtemp(); link.sdir = sd2; sv2 = SN.StateSaver(link, 'SLES-52202', sd2, 10); sv2.save(os.path.join(sd2, 'copy.p2s'), timeout=15); sv2.restore()
        self.assertEqual([f for f in os.listdir(sd2) if f.endswith('.10.p2s')], [])                                                                            # ranura vacía: se deja vacía

if __name__ == '__main__': unittest.main()
