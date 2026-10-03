# Plan H3: validación del integrador fiel

## Objetivo
Demostrar que `src/integrator_fidel.hpp` (`FUN_00238818`) reproduce el integrador del juego, con >=1 función validada contra savestates reales.

## Oráculo ejecutable (rediseñado 2026-10-03: el criterio F_eff=(0,0,-m·g) era falso; ver `docs/formats/integrator.md`)
- Datos: capturas en caída libre de `python3 tools/pcsx2/air_capture.py --rider N --lift 1000 [--omega x y z] [--vel x y z] --out <cap>` (PCSX2 abierto con PINE; el cuerpo solo se integra en ventanas de ~1.2 s), fuera del repo (`~/dh-states/h3/`). 50 ticks consecutivos (1/50 s) por captura.
- Comando: `python3 tools/integrator_check.py <cap> --rider N --assert [--eval <otra.cap> --eval-rider M]` (= `tools/integrator_airfit.py`). Salida 1 si falla el nivel modelo.
- Nivel modelo: modelo `P*=1-c·dt, L*=1-cL·dt, FUN_00238818(F=(0,0,-m·G))`; residuos de |Δpos|, |ΔR|, P y L relativos <= 3 × suelo float32 (pos: ulp del mayor |pos|; R, P, L: ulp(1.0)). Validación cruzada obligatoria (ajustar con una captura, evaluar con otra).
- Nivel procedencia: c, cL, G deben localizarse (RAM/binario). Estado: c, cL = campos `módulo+0x120/0x11C` (confirmado por escritura); G sin localizar (hipótesis A: 3×32.2; B: 3×32.17 rechazada por los datos). Hasta entonces: "ajuste empírico", NO validado.
- Sin ambos niveles no se cierra el plan; el contador de funciones validadas no sube.

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
