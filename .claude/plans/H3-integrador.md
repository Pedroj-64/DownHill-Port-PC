# Plan H3: validación del integrador fiel

## Objetivo
Demostrar que `src/integrator_fidel.hpp` (`FUN_00238818`) reproduce el integrador del juego, con >=1 función validada contra savestates reales.

## Oráculo ejecutable
- Datos: >=3 pares de savestates consecutivos (1 tick = 1/50 s) con la bici en el aire, capturados por PINE: `python3 tools/pcsx2/pine_capture.py --out <dir> [--slot --seconds --no-launch]`.
- Comando: `python3 tools/integrator_check.py <estado_a> <estado_b> --ticks 1`.
- Comando con veredicto: `python3 tools/integrator_check.py <a> <b> --ticks 1 --assert` (o `<captura.cap> --assert`): PASA/FALLA por criterio, salida != 0 si falla (jugador, solo tramos en el aire).
- Criterio de aceptación (aprobado tras el paso 0; float32, tick = 1/50 s): por par, |Δpos| <= 1e-3 u; máx |ΔR_ij| <= 1e-5; F_eff = (P1-P0)/dt con F_z = -m·g (error relativo <= 1e-3) y |F_xy| <= 1e-3·m·g; |T_eff| ≈ 0 (`T_TOL` = 1.0, provisional).
  - m = 100 (invM = 0.01) y 1 u = 1 ft: medidos, `integrator.md` y `docs/p2s-savestates.md`. g = 32.17 u/s² y eje de subida = +z: **hipótesis** (derivadas de 1 u = 1 ft; `integrator.md` solo las "espera").
  - ΔP/ΔL relativos eliminados: vacuos (`integrator_check.py` infiere F y T de ΔP y ΔL, así que P y L coinciden por construcción).
  - Un residuo sistemático en F_eff o T_eff NO es fallo automático: dispara el paso 3 (diagnóstico por sub-función).
  - Estos umbrales se recalibran con los primeros pares reales.
- Sin pares válidos no hay veredicto: el plan no se cierra.

## Pasos (uno por invocación)
0. (hecho) Contraste: leer `docs/formats/integrator.md`, corregir el criterio de este plan y probar `pine_capture.py --help` / `integrator_check.py --help`. Archivos permitidos: este plan. Verificación: criterio final anotado aquí.
0b. (hecho) Oráculo con veredicto: `--assert` en `tools/integrator_check.py` + `tests/test_integrator_check.py`. Verificación: `python3 -m unittest discover tests`, `sh tools/guard.sh`.
1. Captura automática (agente `savestate-validator`): primero `python3 tools/pcsx2/pine_capture.py --no-launch --out ~/dh-states/h3 --seconds <s>` (PCSX2 ya abierto con PINE); si no hay PINE, lanzarlo con `--iso "Downhill Domination.iso" --slot <n> --out ~/dh-states/h3`. Elegir tramos con la bici en el aire. Solo si ambas vías fallan, escalar al usuario con el error exacto. Archivos permitidos: `~/dh-states/` (fuera del repo). Verificación: >=3 pares y `python3 tools/integrator_check.py <par> --ticks 1` carga sin error.
2. Comparación: ejecutar el oráculo en todos los pares (agente `savestate-validator`). Archivos: ninguno. Verificación: tabla de cifras vs criterio.
3. Solo si falla: diagnóstico por sub-función de `FUN_00238818` (`re-analyst`/`porter`). Archivos: `src/integrator_fidel.hpp`, `tests/integrator_test.cpp`. Verificación: `cmake --build build && ctest --test-dir build -R integrator`.
4. Docs: es/en en `docs/formats/integrator.md` y `docs/progress.json` (`doc-writer`). Archivos: esos dos. Verificación: `python3 tools/progress.py` y `sh tools/guard.sh`.

## Prueba PINE (2026-10-03, hecha en el paso 0b)
- Lanzado con `flatpak run net.pcsx2.PCSX2 -batch -fastboot -- <iso>`: el socket aparece en 1 s en `/run/user/1000/.flatpak/net.pcsx2.PCSX2/xdg-run/pcsx2.sock` (ruta confirmada); responde (`SLES-52202`, estado 0); lectura de 16 B OK (ventanas de 4 B devuelven vacío: usar >= 8 B). `flatpak kill` deja el socket huérfano: borrarlo antes de relanzar.
- DEFECTO en `tools/pcsx2/pine_capture.py` (sin tocar, pendiente de OK): con lanzamiento propio llama a `pine.default_socket()` ANTES de que exista el socket y obtiene la ruta de respaldo `/run/user/1000/pcsx2.sock`, así que esperaría 90 s y fallaría. Arreglo: recalcular `default_socket()` dentro del bucle de espera. Con `--no-launch` no afecta.
- Backup de sstates: `~/backups/dh-sstates-2026-10-03/` (15 archivos, 291 MB, fuera del repo). Slots 2-9 no sobrescritos.

## Riesgos
- Estados 01-04/06-09 idénticos entre sí; cuerpo congelado en 05/10: no sirven como pares.
- `node = cm - com·R` falla en estados guardados.
- Sub-pasos con impacto contaminan el par (usar solo tramos en el aire).
- Los umbrales son provisionales hasta los primeros pares reales (g y eje z = hipótesis).

## Escalado
2 fallos seguidos en un paso -> parar y replanificar con Opus (`re-analyst`); no reintentar a ciegas.
