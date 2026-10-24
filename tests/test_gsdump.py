"""Volcado GS sintético (sin datos del juego): cabecera v2 + estado + cola GIF con un sprite plano; comprueba el lector (gsdump.py) y el rasterizador (gsraster.py)."""
import os, struct, sys, tempfile, unittest
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'tools'))
import gsdump, gsraster

def gif_ad(entries):   # etiqueta GIF PACKED con un registro A+D por entrada (EOP=1, NLOOP=n, NREG=1, REGS=0xE)
    tag = struct.pack('<2Q', len(entries) | 1 << 15 | 1 << 60, 0xE)
    return tag + b''.join(struct.pack('<2Q', val, reg) for reg, val in entries)

def build_dump(vsyncs=1):
    ser = b'TEST-00000'; ss = b'\x00' * 4; state = b'\x00' * ((4 << 20) + 16)
    hdr = struct.pack('<9I', 9, len(state), 36, len(ser), 0, 1, 1, 36 + len(ser), len(ss)) + ser + ss
    body = struct.pack('<II', 0xFFFFFFFF, len(hdr)) + hdr + state + b'\x00' * 0x2000
    col = 0x80 << 24 | 40 << 16 | 120 << 8 | 200                                   # RGBA = (200, 120, 40, 0x80)
    xy = lambda x, y: (x * 16) | (y * 16) << 16
    g = gif_ad([(0x4C, 0 | 10 << 16), (0x18, 0), (0x00, 6), (0x01, col), (0x05, xy(10, 20)), (0x05, xy(30, 40))])   # FRAME_1 (FBW 10, CT32), XYOFFSET_1, PRIM sprite, RGBAQ, 2 vértices
    body += b'\x00' + b'\x01' + struct.pack('<I', len(g)) + g
    for _ in range(vsyncs): body += b'\x01\x00'
    return body

class TestGsDump(unittest.TestCase):
    def test_parse_and_raster(self):
        with tempfile.NamedTemporaryFile(suffix='.gs', delete=False) as f: f.write(build_dump()); path = f.name
        try:
            d = gsdump.Dump(path); self.assertEqual(d.serial, 'TEST-00000'); self.assertEqual(len(d.packets), 2)
            rp = gsdump.Replay(d); draws = rp.run(); self.assertEqual(len(draws), 1)
            self.assertEqual(draws[0]['prim'] & 7, 6)
            img = gsraster.render(rp, 0, 0)
            self.assertEqual(tuple(img[25, 15]), (200, 120, 40))                    # dentro del sprite
            self.assertEqual(tuple(img[5, 5]), (0, 0, 0))                           # fuera
        finally: os.unlink(path)

    def test_truncated_stream_rejected(self):
        with tempfile.NamedTemporaryFile(suffix='.gs', delete=False) as f: f.write(build_dump()[:-5]); path = f.name
        try:
            with self.assertRaises(Exception): gsdump.Dump(path)
        finally: os.unlink(path)

if __name__ == '__main__': unittest.main()
