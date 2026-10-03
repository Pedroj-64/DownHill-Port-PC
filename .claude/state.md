# Estado del proyecto (actualizar con /handoff)

## Hito actual
H3 bici: validación del integrador fiel `FUN_00238818` (`src/integrator_fidel.hpp`).
H4 piloto en paralelo, en pausa. Plan: `.claude/plans/H3-integrador.md`.

## Último resultado verificado
- Progreso: 42.9 % verificado / 55.8 % implementado (`docs/progress.json`).
- H1 (mapa) y H2 (colisión) cerrados.
- H3: bici jugable (ALP2 28/28 puertas, 0 reinicios, autopiloto) pero integrador fiel
  0/10 funciones validadas: faltan savestates consecutivos en el aire.
  Parámetros de `bike.hpp` = hipótesis.
- H4: malla, esqueleto, piel y torso decodificados; sin comparar con el juego.
- H5: C++20 cerrado; colisión nativa 1/8 módulos; cargador nativo de malla del
  piloto 13/13 (último commit; progress.json aún no lo cuenta).

## Pasos 0 y 0b de H3 (hechos)
- Plan actualizado (criterios aprobados): |Δpos|<=1e-3, |ΔR|<=1e-5, F_z=-m·g rel<=1e-3, |F_xy|<=1e-3·m·g, T_eff≈0 (T_TOL=1.0 provisional); ΔP/ΔL eliminados. g=32.17 y eje +z = hipótesis.
- `tools/integrator_check.py --assert [--t-tol]` (PASA/FALLA, salida 1) + `tests/test_integrator_check.py`; 76 tests Python OK, guard OK.
- PINE probado: socket real `/run/user/1000/.flatpak/net.pcsx2.PCSX2/xdg-run/pcsx2.sock`, responde. Backup en `~/backups/dh-sstates-2026-10-03/`.
- DEFECTO pendiente de OK: `pine_capture.py` sin `--no-launch` resuelve el socket antes de que exista (ver plan, sección PINE).
- Agente `savestate-validator` carga bien.

## Próximas 3 tareas
1. Con OK del usuario: arreglar `pine_capture.py` (recalcular `default_socket()` en el bucle de espera).
2. Paso 1: dejar la bici en el aire (slot de caída larga de ALP2) y capturar (`--seconds` corto, `--out` fuera del repo; no sobrescribir slots 2-9 sin backup).
3. Paso 2: `python3 tools/integrator_check.py <air.cap> --ticks 1 --assert`; recalibrar umbrales; doc-writer + `progress.py`.

## Hipótesis abiertas (cómo validarlas)
- Parámetros de `bike.hpp`: comparar contra el integrador fiel con pares en el aire (H3).
- `node = cm - com·R` falla en estados guardados: probar con pares de H3, sub-función por sub-función.
- Regla de meta 8052 y niebla: script contra los 54 niveles (`/re-format`).
- Fuente de pose del cuerpo del piloto (array RAM sin localizar), piernas por IK, anclaje `DH_RIDER_AT`:
  `tools/compare_view.py` contra captura + `tests.test_rider`.

## Bloqueos
- Ninguno activo. Si la captura PINE automática falla (paso 1 del plan), escalar al usuario con el error.
