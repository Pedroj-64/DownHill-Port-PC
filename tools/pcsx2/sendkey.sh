#!/bin/sh
# SPDX-FileCopyrightText: 2026 Pedro Soto
# SPDX-License-Identifier: GPL-3.0-or-later
# Manda UNA tecla al PCSX2 anidado (ver nested.sh). Nunca toca la sesión del usuario: se niega si el
# destino no es el socket del compositor anidado o si coincide con el real.
# Uso: sendkey.sh [-m ctrl|shift|alt] <tecla>        p. ej.: sendkey.sh F8   |   sendkey.sh -m shift F1
# Comprueba el efecto con shot.sh (captura del compositor anidado) o mirando el archivo que debía crearse.
set -eu
RT=${XDG_RUNTIME_DIR:-/run/user/$(id -u)}
STATE="$RT/dh-nested"
REAL=${WAYLAND_DISPLAY:-wayland-0}
mod=""
[ "${1:-}" = "-m" ] && { mod=$2; shift 2; }
key=${1:?tecla}
case "$mod" in ""|ctrl|shift|alt) ;; *) echo "modificador no permitido" >&2; exit 2;; esac
printf '%s' "$key" | grep -Eq '^(F([1-9]|1[0-2])|[a-zA-Z0-9]|Return|Escape|space|Tab|Up|Down|Left|Right)$' || { echo "tecla no permitida: $key" >&2; exit 2; }
[ -f "$STATE" ] && kill -0 "$(sed -n 1p "$STATE")" 2>/dev/null || { echo "no hay PCSX2 anidado (nested.sh start)" >&2; exit 1; }
nested=$(sed -n 2p "$STATE")
[ "$nested" != "$REAL" ] && [ -S "$RT/$nested" ] || { echo "destino inválido ($nested)" >&2; exit 1; }
# El teclado virtual solo existe mientras corre wtype: hay que darle tiempo al cliente (Qt) para enganchar
# wl_keyboard antes de la tecla (-s antes) y no cerrarlo de golpe (-s después). Ajustable con DH_KEY_WAIT (ms).
w=${DH_KEY_WAIT:-600}
if [ -n "$mod" ]; then
  # PCSX2 enlaza el modificador como TECLA (Keyboard/Shift): hay que pulsarlo de verdad (-P/-p), no solo -M.
  case "$mod" in shift) mk=Shift_L;; ctrl) mk=Control_L;; alt) mk=Alt_L;; esac
  WAYLAND_DISPLAY=$nested wtype -s "$w" -P "$mk" -s 250 -P "$key" -s 150 -p "$key" -s 150 -p "$mk" -s 150
else
  if [ -n "${DH_KEY_HOLD:-}" ]; then WAYLAND_DISPLAY=$nested wtype -s "$w" -P "$key" -s "$DH_KEY_HOLD" -p "$key" -s 150   # tecla mantenida (botones del mando que piden varios fotogramas)
  else WAYLAND_DISPLAY=$nested wtype -s "$w" -k "$key" -s 150; fi
fi
echo "tecla $mod$key -> $nested"
