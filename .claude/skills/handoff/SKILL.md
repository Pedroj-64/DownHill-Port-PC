---
name: handoff
description: Usar al terminar una sesión de trabajo (/handoff) para dejar el estado listo para la siguiente. Actualiza .claude/state.md y llama a doc-writer si cambió el progreso.
---
1. Actualiza `.claude/state.md` (máx. 40 líneas): hecho, descartado, próximas 3 tareas, hipótesis abiertas (con cómo validarlas), bloqueos.
2. Si cambió el progreso verificado, invoca el subagente `doc-writer` para `docs/progress.json` y ejecuta tú `python3 tools/progress.py`.
3. No hagas commit ni push; recuerda al usuario si hay cambios sin commitear.
