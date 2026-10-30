# SPDX-FileCopyrightText: 2026 Pedro Soto
# SPDX-License-Identifier: GPL-3.0-or-later
# tools/sweep_levels.py con registros SINTÉTICOS (sin datos del juego): lectura del registro, lista de niveles, tabla y detección de regresiones.
import os, sys, unittest
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'tools'))
import sweep_levels as sw

OK = """carga nativa desde la ISO: X (16 archivos virtuales)
peatones: 24 puntos, 12 modelos (3 tipos)
rivales: 5 modelos de piloto
salida: pos=(1 2 3) rumbo=0.01 startPts=1 startPt=(0 0 0)
colisión: 136176 triángulos (X.col)
sfx resumen: ticks=300 t=5.0 s puertas=2/28 reinicios=0 vel máx=80"""

class T(unittest.TestCase):
    def test_parse_ok(self):
        r = sw.parse_log(OK)
        self.assertEqual((r['collision'], r['start_grid'], r['rivals'], r['pedestrians'], r['gates'], r['status']), (136176, 1, 5, 24, '2/28', 'OK'))
    def test_missing_lines_and_errors(self):
        r = sw.parse_log('colisión: 10 triángulos (X.col)\nrider approx: errores máx. de la fase 0: 0.000\nPTR/TEX inválido')
        self.assertEqual((r['rivals'], r['start_grid'], r['status']), (0, None, 'ERROR'))   # «errores máx.» no cuenta; «inválido» sí
        self.assertEqual(sw.parse_log('', 0, True)['status'], 'TIMEOUT')
        self.assertEqual(sw.parse_log('rivales: 5 modelos', 1)['status'], 'ERROR')            # sin colisión y código de salida != 0
    def test_list_levels(self):
        out = '7247742 c9b401b9 LVL/ALP2.NGP\n10 aa LVL/ALP2.PTR\n5 bb LVL/PODARC.NGP\n9 cc LVL/MOAB.NGP\n4 dd SHELL/UI.NGP\nDIR BIKE'
        self.assertEqual(sw.list_levels(out), ['ALP2', 'MOAB'])
    def test_table_and_regressions(self):
        a = sw.parse_log(OK); b = dict(a, rivals=3, start_grid=0, collision=0, errors=['PTR/TEX inválido'], status='ERROR')
        t = sw.table({'ALP2': a, 'MOAB': b}); self.assertIn('| ALP2 | 136176 | sí | 5 | 24 | 2/28 | OK |', t); self.assertIn('ERROR (PTR/TEX inválido)', t)
        reg = sw.regressions({'ALP2': a, 'FUJI': a}, {'ALP2': b})
        for frag in ('ALP2: ya no carga la colisión', 'ALP2: ya no hay rejilla de salida', 'ALP2: menos rivales (5 -> 3)', 'ALP2: estado OK -> ERROR', 'ALP2: error nuevo', 'FUJI: falta en el barrido nuevo'):
            self.assertTrue(any(frag in x for x in reg), frag)
        self.assertEqual(sw.regressions({'ALP2': a}, {'ALP2': dict(a, rivals=8)}), [])      # mejorar no es regresión
    def test_partial_baseline(self):
        import json, tempfile
        a = sw.parse_log(OK); f = tempfile.NamedTemporaryFile('w', suffix='.json', delete=False); json.dump({'ALP2': a, 'MOAB': a}, f); f.close()
        calls = []
        orig = sw.run_level; sw.run_level = lambda d, i, n, t, to: (calls.append(n), dict(a, rivals=3))[1]
        try: rc = sw.main(['x.iso', '--levels', 'ALP2', '--baseline', f.name, '--jobs', '1'])
        finally: sw.run_level = orig; os.unlink(f.name)
        self.assertEqual((rc, calls), (1, ['ALP2']))   # solo ALP2 (menos rivales); MOAB no se ejecutó y no cuenta como regresión

if __name__ == '__main__': unittest.main()
