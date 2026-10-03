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

## H3: pasos 1 y 2 hechos (caída libre fabricada); oráculo del plan INADECUADO -> pendiente decisión del usuario
- Hallazgo: el módulo físico (rider+0x6420) solo se integra en tramos de ~1.2 s (impactos/"GOLPE"; enlace `+0x6428` = nodo); el resto del tiempo el juego mueve el nodo por otra vía. `tools/pcsx2/air_capture.py` espera ese enlace y SUBE el cuerpo (+z) por PINE; el juego integra solo (nodo coherente).
- Caídas: `~/dh-states/h3/air_r1b.cap` (+1000), `air_r1_L400/L2500.cap`, `air_r5.cap` (rider 5): 50 pasos consecutivos cada una. `air_r1.cap` = intento fallido (poke sobrescrito).
- `--assert` del plan FALLA en todos (|Δpos| 3.9e-2, |ΔR| 7e-4, F_z, F_xy, T): el supuesto "en el aire F_eff = (0,0,-m·g)" es FALSO. Modelo ajustado (residuo 2e-6): antes de integrar el juego amortigua P *= 1-1.25·dt y L *= 1-0.65·dt; gravedad F=(0,0,-m·96.6) entra en FUN_00238818 (pos y R usan la velocidad/omega ya amortiguadas; P += dt·F después).
- `tools/integrator_airfit.py`: con ese modelo, 50 pasos: |Δpos| <= 9.7e-4 (suelo float32 = ulp 9.8e-4), |ΔR| 1.7e-7, P/L rel 3e-8..7e-8. Mismas constantes en 2 pilotos y 3 alturas (G=96.6 u/s2: 1 u ≈ 0.1 m, no 1 ft: hipótesis a revisar).
- Commits: af14d6a, 7766e2c, 55f29e5, fd580bc, 595b415. Sin push. Backup y sstates intactos.

## Próximas 3 tareas
1. Usuario: aprobar rediseñar el oráculo (plan paso 0b): sustituir F_eff/T_eff por el modelo de integrator_airfit (pos<=1e-3 (ulp), R<=1e-5, P/L rel<=1e-6) y marcar c, cL, G como constantes del juego (hipótesis hasta hallarlas en el binario).
2. Paso 4: docs es/en en `docs/formats/integrator.md` + `docs/progress.json` (doc-writer) y `python3 tools/progress.py`.
3. Localizar c=125, cL=0.65, G=96.6 en el binario (¿FUN_xxxxxxxx que amortigua antes de FUN_00238818?) y validar con otro estado (jugador al chocar).

## Hipótesis abiertas (cómo validarlas)
- Parámetros de `bike.hpp`: comparar contra el integrador fiel con pares en el aire (H3).
- `node = cm - com·R` falla en estados guardados: probar con pares de H3, sub-función por sub-función.
- Regla de meta 8052 y niebla: script contra los 54 niveles (`/re-format`).
- Fuente de pose del cuerpo del piloto (array RAM sin localizar), piernas por IK, anclaje `DH_RIDER_AT`:
  `tools/compare_view.py` contra captura + `tests.test_rider`.

## Bloqueos
- Ninguno activo. Si la captura PINE automática falla (paso 1 del plan), escalar al usuario con el error.
