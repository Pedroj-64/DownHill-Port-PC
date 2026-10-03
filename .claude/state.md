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

## Próximas 3 tareas
1. Paso 0 de H3: contrastar el criterio numérico con `docs/formats/integrator.md` y probar la captura en seco.
2. Capturar >=3 pares de savestates consecutivos con la bici en el aire (automático con `pine_capture.py --no-launch`, o lanzando PCSX2 con `--iso --slot`; escalar solo si falla).
3. `python3 tools/integrator_check.py <par> --ticks 1`; con el veredicto, actualizar progress.json (`doc-writer`) y regenerar con `python3 tools/progress.py`.

## Hipótesis abiertas (cómo validarlas)
- Parámetros de `bike.hpp`: comparar contra el integrador fiel con pares en el aire (H3).
- `node = cm - com·R` falla en estados guardados: probar con pares de H3, sub-función por sub-función.
- Regla de meta 8052 y niebla: script contra los 54 niveles (`/re-format`).
- Nodo tipo 17 (FUJI): script de validación sobre niveles que lo contienen.
- Fuente de pose del cuerpo del piloto (array RAM sin localizar), piernas por IK, anclaje `DH_RIDER_AT`:
  `tools/compare_view.py` contra captura + `tests.test_rider`.

## Bloqueos
- Ninguno activo. Si la captura PINE automática falla (paso 1 del plan), escalar al usuario con el error.
- H6 sin oráculo; candidato al llegar: prueba de humo sin interfaz (nivel + autopiloto + estados hasta meta).
- Riesgo: estados 01-04/06-09 idénticos; cuerpo congelado en 05/10 (no sirven como pares).
