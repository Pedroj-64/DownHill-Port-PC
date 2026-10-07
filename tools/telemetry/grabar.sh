#!/bin/sh
# Doble clic / ejecutar: graba solo (nivel, piloto y guion automáticos). Linux y macOS. / Double-click or run: records by itself.
cd "$(dirname "$0")/../.." || exit 1
if command -v python3 >/dev/null 2>&1; then python3 tools/telemetry/record.py "$@"; else echo "Instala Python 3 / Install Python 3: https://www.python.org/downloads/"; fi
printf '\nPulsa Enter para cerrar / Press Enter to close... '; read -r _
