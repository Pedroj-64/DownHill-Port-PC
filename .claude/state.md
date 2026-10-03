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

## H3: oráculo de dos niveles hecho; falta procedencia de G y paso 4 cerrado (docs)
- Oráculo rediseñado (plan actualizado): `integrator_check.py <cap> --rider N --assert [--eval otra.cap --eval-rider M]` = `integrator_airfit.py`. Nivel modelo: umbrales 3×suelo float32 (pos 2.93e-3; R,P,L 3.58e-7); validación cruzada r1<->r5 PASA (<=1.0 ulp pos, R 1.4-1.7e-7), también con ω y v laterales escritas.
- Procedencia: c, cL = `módulo+0x120` (0.975) / `+0x11C` (0.987), confirmado escribiendo (c=2.5, cL=5.0). G=96.600±1e-4 NO localizada: 3×32.17 rechazada (~900σ), 98.1 rechazada, `módulo+0x124`=3.0 NO es el multiplicador; única `32.2f` en código: 0x136374 (fn 0x13634C), hipótesis sin probar. Todo "ajuste empírico", NO validado; contador de funciones sin cambios (progress.json: nota "Parcial").
- Regla nueva (CLAUDE.md 9): sin uinput/evdev ni robar foco sin permiso. Capturas en `~/dh-states/h3/` (air_*.cap, prov_*.cap). Backup y sstates intactos.
- Commits: ebda174 d643d81 cce18e4 690062a 8f67e2f (+ anteriores). Sin push.

## Próximas 3 tareas
1. Localizar G: probar escribiendo candidatas (global 0x77A758, campos `+0x418/+0x150` del objeto de 0x13634C) y ver si G del ajuste cambia; luego citar la FUN_xxxxxxxx.
2. Localizar la función que aplica los factores `+0x11C/+0x120` (escritura/lectura en RAM o desensamblado) y su orden respecto a FUN_00238818.
3. Con ambos niveles: marcar validada la función en progress.json (doc-writer + progress.py) y validar con otro caso (jugador al chocar).

## Hipótesis abiertas (cómo validarlas)
- Parámetros de `bike.hpp`: comparar contra el integrador fiel con pares en el aire (H3).
- `node = cm - com·R` falla en estados guardados: probar con pares de H3, sub-función por sub-función.
- Regla de meta 8052 y niebla: script contra los 54 niveles (`/re-format`).
- Fuente de pose del cuerpo del piloto (array RAM sin localizar), piernas por IK, anclaje `DH_RIDER_AT`:
  `tools/compare_view.py` contra captura + `tests.test_rider`.

## Bloqueos
- Ninguno activo. Si la captura PINE automática falla (paso 1 del plan), escalar al usuario con el error.
