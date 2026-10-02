#!/bin/sh
# SPDX-FileCopyrightText: 2026 Pedro Soto
# SPDX-License-Identifier: GPL-3.0-or-later
# Revisa TODOS los archivos versionados: material del juego, archivos grandes, rutas personales y secretos.
# Lo usa el CI (también protege PRs de quien no active el hook local).  Uso: tools/guard.sh
fail=0
files=$(git ls-files)
bad=$(printf '%s\n' "$files" | grep -iE '\.(iso|bin|cue|img|elf|irx|ngp|ptr|rtx|tex|pts|rst|bnk|vpk|vag|pss|orb|rep|nga|skx|ctl|ico|gpr|pem|key)$|(^|/)(SLES|SCES|SLUS)_|(^|/)(iso_extract|unpacked|decomp|ghidra_projects|assets)/|\.env$')
[ -n "$bad" ] && { echo "Material del juego o secretos versionados:"; echo "$bad"; fail=1; }
for f in $files; do
  [ -f "$f" ] || continue
  [ "$(wc -c < "$f")" -gt 1048576 ] && { echo "Archivo > 1 MiB: $f"; fail=1; }
done
hits=$(printf '%s\n' "$files" | grep -v '^LICENSE$' | xargs grep -nIE '/home/[a-z0-9_-]+/|/Users/[A-Za-z0-9_-]+/|BEGIN (RSA|OPENSSH|EC|PRIVATE)|(api[_-]?key|secret|token|passwd|password)[[:space:]]*[:=]' 2>/dev/null | grep -v 'tools/guard.sh\|.githooks/')
[ -n "$hits" ] && { echo "Ruta personal o posible secreto:"; echo "$hits" | head -10; fail=1; }
[ $fail -eq 0 ] && echo "guard: OK"
exit $fail
