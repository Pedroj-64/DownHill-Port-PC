# SPDX-FileCopyrightText: 2026 Pedro Soto
# SPDX-License-Identifier: GPL-3.0-or-later
import os, sys, tempfile, unittest
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'tools', 'pcsx2'))
import pine_capture

class WaitSocket(unittest.TestCase):
    def test_path_changes_after_launch(self):
        with tempfile.TemporaryDirectory() as d:
            real, fallback, calls = os.path.join(d, 'real.sock'), os.path.join(d, 'fallback.sock'), []
            def resolve():
                calls.append(1)
                if len(calls) == 3: open(real, 'w').close()    # el socket aparece en la 3ª vuelta
                return real if os.path.exists(real) else fallback
            self.assertEqual(pine_capture.wait_socket(5, resolve, 0.01), real)
    def test_timeout(self):
        with self.assertRaises(SystemExit): pine_capture.wait_socket(0.05, lambda: '/nonexistent/x.sock', 0.01)

class CheckRiders(unittest.TestCase):
    def test_range(self):
        self.assertEqual(pine_capture.check_riders(10, 6), 6); self.assertEqual(pine_capture.check_riders(10, 10), 10); self.assertEqual(pine_capture.check_riders(1, 6), 1)
        for bad in (0, 11, 0xFFFFFFFF):
            with self.assertRaises(SystemExit): pine_capture.check_riders(bad, 6)

if __name__ == '__main__': unittest.main()
