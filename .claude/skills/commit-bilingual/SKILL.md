---
name: commit-bilingual
description: Usar solo cuando el usuario pida preparar o hacer un commit (/commit-bilingual). Mensaje es/en, guard.sh y confirmación; nunca push ni trailer de co-autor.
---
1. `git status` y `git diff --stat`; el commit debe ser pequeño (si no, propón dividirlo).
2. Redacta el mensaje: `area: english / área: español` (como el `git log`).
3. Ejecuta `sh tools/guard.sh`; si falla, detente.
4. Muestra el mensaje y ESPERA confirmación del usuario antes de `git commit`.
5. Sin `Co-Authored-By` ni mención a Claude. Nunca `git push`.
