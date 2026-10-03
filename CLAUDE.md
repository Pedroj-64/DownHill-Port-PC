# Downhill Domination: port PS2 -> PC (C++20 + herramientas Python de referencia)

## Arranque
Lee `.claude/state.md` antes que nada; no explores el repo salvo que el plan lo pida.
Planes por hito en `.claude/plans/` (plantilla: `TEMPLATE.md`). Índice de formatos: `docs/formats/INDEX.md`.

## Propósito
Port nativo de Downhill Domination (PS2). Python (`tools/`) = referencia y oráculo;
C++20 (`src/`) = port nativo, validado bit a bit contra la referencia Python.
Progreso: `docs/progress.json` (se regenera con `python3 tools/progress.py`). Decisiones: `docs/DECISIONS.md`.

## Comandos reales
- Configurar: `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release`
- Compilar: `cmake --build build`
- Tests C++: `ctest --test-dir build` (ground, contact, bike, integrator, rider, rider_mesh, nga)
- Tests Python: `python3 -m unittest discover tests` (pytest NO está instalado)
- Guardia de datos: `sh tools/guard.sh`
- Progreso: `python3 tools/progress.py`; entorno: `python3 tools/dh.py doctor`
- Oráculo H3: `python3 tools/integrator_check.py <estados...> --ticks N`
- Captura PINE: `python3 tools/pcsx2/pine_capture.py --out <dir> [--slot --seconds --no-launch]`
- Oráculo H4: `python3 tools/compare_view.py` + `python3 -m unittest tests.test_rider`

## Reglas duras
1. Commits solo cuando el usuario lo pida; pequeños y bilingües, formato
   `area: english / área: español` (como el `git log`).
2. Nunca `git push`.
3. Sin trailer `Co-Authored-By` ni mención a Claude: los commits van solo a nombre del usuario.
4. Ningún dato del juego entra al repo (ISO, assets, volcados de memoria, salida del
   decompilador). Respeta `tools/guard.sh` y `.githooks/` (hook: `.claude/hooks/block_game_data.py`).
5. Docs de hallazgos en `docs/` siempre en español e inglés.
6. Cita la función como `FUN_xxxxxxxx` o márcalo como "hipótesis"; una hipótesis no es un hecho
   sin un script de validación ejecutado contra los 54 niveles.
7. Nada de Gemini ni otras IAs externas: todo con Claude.
8. Un paso del plan por invocación; el plan debe tener oráculo ejecutable.

## Flujo
- `/status` dónde estamos; `/plan-milestone` nuevo plan; `/re-format` hipótesis de formato;
  `/commit-bilingual` preparar commit; `/handoff` cerrar sesión (actualiza `state.md`).
- Subagentes (`.claude/agents/`): `re-analyst` (opus), `porter` (sonnet),
  `savestate-validator` (sonnet), `doc-writer` (haiku).
- Escalado: 2 fallos seguidos en un paso -> replanificar con Opus.
- `.claude/` está en `.gitignore` (línea 57): esta configuración no se versiona.
