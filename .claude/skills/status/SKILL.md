---
name: status
description: Usar cuando el usuario pregunte dónde estamos, qué sigue o el estado del proyecto Downhill (/status). Lee solo .claude/state.md y un resumen de docs/progress.json.
---
1. Lee `.claude/state.md`.
2. Resume `docs/progress.json` (porcentajes e hitos) con `python3 -c "import json;d=json.load(open('docs/progress.json'));print(list(d)[:10])"` o lectura corta.
3. Responde: hito actual, último resultado, próximas 3 tareas, bloqueos. No explores nada más.
