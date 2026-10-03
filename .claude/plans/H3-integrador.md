# Plan H3: validación del integrador fiel

## Objetivo
Demostrar que `src/integrator_fidel.hpp` (`FUN_00238818`) reproduce el integrador del juego, con >=1 función validada contra savestates reales.

## Oráculo ejecutable
- Datos: >=3 pares de savestates consecutivos (1 tick = 1/50 s) con la bici en el aire, capturados por PINE: `python3 tools/pcsx2/pine_capture.py --out <dir> [--slot --seconds --no-launch]`.
- Comando: `python3 tools/integrator_check.py <estado_a> <estado_b> --ticks 1`.
- Criterio de aceptación (PROPUESTA mía, a contrastar con `docs/formats/integrator.md` en el paso 0; ajustable): por tick, |Δpos| <= 1e-3 u; máx |ΔR_ij| <= 1e-4; |ΔP| y |ΔL| relativos <= 1e-4 (ruido float32).
- Sin pares válidos no hay veredicto: el plan no se cierra.

## Pasos (uno por invocación)
0. Contraste: leer `docs/formats/integrator.md`, corregir el criterio de este plan y probar `pine_capture.py --help` / `integrator_check.py --help`. Archivos permitidos: este plan. Verificación: criterio final anotado aquí.
1. Captura automática (agente `savestate-validator`): primero `python3 tools/pcsx2/pine_capture.py --no-launch --out ~/dh-states/h3 --seconds <s>` (PCSX2 ya abierto con PINE); si no hay PINE, lanzarlo con `--iso "Downhill Domination.iso" --slot <n> --out ~/dh-states/h3`. Elegir tramos con la bici en el aire. Solo si ambas vías fallan, escalar al usuario con el error exacto. Archivos permitidos: `~/dh-states/` (fuera del repo). Verificación: >=3 pares y `python3 tools/integrator_check.py <par> --ticks 1` carga sin error.
2. Comparación: ejecutar el oráculo en todos los pares (agente `savestate-validator`). Archivos: ninguno. Verificación: tabla de cifras vs criterio.
3. Solo si falla: diagnóstico por sub-función de `FUN_00238818` (`re-analyst`/`porter`). Archivos: `src/integrator_fidel.hpp`, `tests/integrator_test.cpp`. Verificación: `cmake --build build && ctest --test-dir build -R integrator`.
4. Docs: es/en en `docs/formats/integrator.md` y `docs/progress.json` (`doc-writer`). Archivos: esos dos. Verificación: `python3 tools/progress.py` y `sh tools/guard.sh`.

## Riesgos
- Estados 01-04/06-09 idénticos entre sí; cuerpo congelado en 05/10: no sirven como pares.
- `node = cm - com·R` falla en estados guardados.
- Sub-pasos con impacto contaminan el par (usar solo tramos en el aire).
- El criterio numérico es una suposición hasta el paso 0.

## Escalado
2 fallos seguidos en un paso -> parar y replanificar con Opus (`re-analyst`); no reintentar a ciegas.
