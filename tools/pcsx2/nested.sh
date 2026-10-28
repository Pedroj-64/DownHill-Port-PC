#!/bin/sh
# SPDX-FileCopyrightText: 2026 Pedro Soto
# SPDX-License-Identifier: GPL-3.0-or-later
# PCSX2 en un compositor anidado SIN ventana en el escritorio del usuario (cage + backend headless).
# Las teclas de sendkey.sh solo llegan a ese compositor: nunca a la sesión real. Requiere: cage, wtype, grim, flatpak.
# Uso: nested.sh start [ISO] | stop | status | display
set -eu
RT=${XDG_RUNTIME_DIR:-/run/user/$(id -u)}
STATE="$RT/dh-nested"          # líneas: pid, nombre del socket Wayland anidado
REAL=${WAYLAND_DISPLAY:-wayland-0}
ISO=${2:-}

alive() { [ -f "$STATE" ] && kill -0 "$(sed -n 1p "$STATE")" 2>/dev/null; }

case "${1:-status}" in
start)
  command -v cage >/dev/null || { echo "falta cage: sudo pacman -S --needed cage" >&2; exit 1; }
  alive && { echo "ya en marcha: $(sed -n 2p "$STATE")"; exit 0; }
  before=$(ls "$RT" | grep -E '^wayland-[0-9]+$' || true)
  # headless: no crea ventana en el escritorio real. gles2 sobre un render node para que Vulkan/GL de PCSX2 funcione.
  WLR_BACKENDS=headless WLR_LIBINPUT_NO_DEVICES=1 WLR_RENDERER=${DH_WLR_RENDERER:-gles2} \
  WLR_RENDER_DRM_DEVICE=${DH_RENDER_NODE:-/dev/dri/renderD128} \
  nohup cage -- flatpak run ${DH_FLATPAK_ARGS:-} net.pcsx2.PCSX2 -nogui ${DH_PCSX2_ARGS:-} ${ISO:+-- "$ISO"} >"$RT/dh-nested.log" 2>&1 &
  pid=$!
  i=0
  while [ $i -lt 40 ]; do
    sock=$(ls "$RT" | grep -E '^wayland-[0-9]+$' | grep -vxF "$before" | head -1 || true)
    [ -n "$sock" ] && break
    kill -0 $pid 2>/dev/null || { cat "$RT/dh-nested.log" >&2; echo "cage murió" >&2; exit 1; }
    sleep 0.25; i=$((i+1))
  done
  [ -n "${sock:-}" ] || { echo "no apareció el socket anidado" >&2; kill $pid; exit 1; }
  [ "$sock" != "$REAL" ] || { echo "socket anidado == sesión real, abortando" >&2; kill $pid; exit 1; }
  printf '%s\n%s\n' "$pid" "$sock" >"$STATE"
  echo "anidado: $sock (pid $pid)";;
stop)
  alive && kill "$(sed -n 1p "$STATE")" || true
  rm -f "$STATE";;
display)
  alive || { echo "no hay compositor anidado" >&2; exit 1; }
  sed -n 2p "$STATE";;
status)
  if alive; then echo "activo: $(sed -n 2p "$STATE") pid $(sed -n 1p "$STATE")"; else echo "parado"; fi;;
*) echo "uso: nested.sh start [ISO]|stop|status|display" >&2; exit 2;;
esac
