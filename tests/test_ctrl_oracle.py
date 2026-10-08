import os, sys, unittest
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'tools', 'telemetry'))
import ctrl_oracle as O

class Model(unittest.TestCase):          # valores medidos en la telemetría de un colaborador (docs/telemetry-awiiwa.md)
    def test_ground(self):
        self.assertAlmostEqual(O.model(0, 127, True, O.PAR), 0.00649385, places=7)      # primer paso con giro analógico a fondo
        self.assertAlmostEqual(O.model(0, 1, True, O.PAR), 0.00654498, places=7)        # digital = 0.75
        self.assertAlmostEqual(O.model(0.02, 127, True, O.PAR), 0.024677, places=6)     # tope max*127/128*0.75
        self.assertEqual(O.model(0.02, 0, True, O.PAR), 0.0)                            # suelta: se para de golpe
    def test_clamp_rule(self):                                                             # FUN_00137C48: tope fijo, y al pasarlo vale tope*u
        self.assertAlmostEqual(O.model(-0.024677, -9, True, O.PAR), -0.001749, places=6)    # entrada parcial: salta a max*u
        self.assertAlmostEqual(O.model(-0.024677, -3, True, O.PAR), -0.02483, places=5)      # no pasa el tope: sigue sumando
    def test_trick_boost(self):                                                            # ctrl+0x49C = 2.8 (izquierda+derecha al cargar el salto)
        self.assertAlmostEqual(O.model(0.0, 127, False, O.PAR, 2.8), 0.005235988 * 2.8 * 127 / 128, places=9)
    def test_air(self):
        self.assertAlmostEqual(O.model(0.01314, 127, False, O.PAR), 0.01314 + 0.005235988 * 127 / 128, places=9)
        self.assertAlmostEqual(O.model(0.01939, 0, False, O.PAR), 0.01309, places=5)    # decaimiento 0.675
        self.assertEqual(O.model(0.00017, 0, False, O.PAR), 0.0)

if __name__ == '__main__': unittest.main()

class Pitch(unittest.TestCase):
    def test_pitch(self):
        self.assertAlmostEqual(O.pitch_model(0, -1, True), -0.008726646, places=9)      # arriba: -acc, sin importar la magnitud de la pulsación
        self.assertAlmostEqual(O.pitch_model(0.04, 1, True), 0.046076693, places=9)     # tope
        self.assertAlmostEqual(O.pitch_model(0.02, 0, True), 0.016, places=9)           # decae *0.8
        self.assertEqual(O.pitch_model(0.02, 1, False), 0.0)                            # en suelo no hay cabeceo
