#!/bin/sh
# SPDX-FileCopyrightText: 2026 Pedro Soto
# SPDX-License-Identifier: GPL-3.0-or-later
# Captura PNG del compositor anidado de PCSX2 (lo que "ve" el juego, sin molestar al usuario). Uso: shot.sh salida.png
set -eu
RT=${XDG_RUNTIME_DIR:-/run/user/$(id -u)}
nested=$(sh "$(dirname "$0")/nested.sh" display)
WAYLAND_DISPLAY=$nested grim "${1:?salida.png}"
