# Telemetría de un colaborador: hallazgos / Contributor telemetry: findings

Datos: 4 carreras (ALPINE 10 pilotos, ALPINEMX 6, ALPINET 8, MOAB 10) y un recorrido de menús, SLES-52202, ~42 muestras/s. Los volcados viven fuera del repo (`~/dh-states/`). / Data: 4 races + a menu tour, SLES-52202, ~42 samples/s. Dumps stay outside the repo.

## ES
- **Paso de física = 0,02 s (50 Hz)**: en ~93-95 % de los pares de muestras consecutivas |ΔP|/|v| = 1 paso exacto; el resto salta 1-3 pasos. Unos 12-13 mil pares limpios (estado de `ctrl` + entradas → estado siguiente) por carrera: oráculo para el controlador de conducción (FUN_00136330 y vecinas), hasta ahora solo había capturas sueltas. Hipótesis hasta usarlo en un script de validación.
- **Salto**: `jump_obj` (`rider+0x5E40`) `+0x0C` = 1 mientras se carga el botón, 2 en el fotograma de soltar, 0 después; `+0x08` = 1 desde que se pulsa hasta ~0,1 s tras aterrizar. Es independiente del mando. El `rider_input+0x38` que leía `detect.py` es otro botón (3 pulsaciones en ALPINE, 0 en MOAB y ALPINET con decenas de saltos); la pulsación de salto aparece en una palabra de `rider_input` distinta según la fuente de entrada (en ALPINEMX `+0x28`, valor analógico 0-255 con espejos `+0x54/+0x1B8/+0x1E4`). Tiempos de carga vistos: 0,1-2,5 s.
- **Detector**: `jump_short`/`jump_long` usaban `+0x38` → 0 cubiertos. Ahora leen `jump_obj+0x0C`; corto < 0,4 s, largo ≥ 0,6 s (antes la franja 0,4-0,8 no contaba). Repetidas las 4 carreras con `Coach`: guion completo (antes faltaban saltos, pedal y tiempo).
- `coherente: False` en carreras de 6 y 8 pilotos: la lista de pilotos (`0x7684A0`) siempre trae 10 entradas; el comprobador exigía igualdad con el nº de pilotos. Ahora basta `nº <= entradas`.
- Pedal: 10 s seguidos a tope no se consigue (la resistencia no da); el objetivo pasa a 5 s acumulados; tiempo 150 s.

## EN
- **Physics step = 0.02 s (50 Hz)**: in ~93-95 % of consecutive sample pairs |ΔP|/|v| equals exactly one step; the rest skip 1-3 steps. About 12-13 k clean pairs (`ctrl` state + inputs → next state) per race: an oracle for the riding controller (FUN_00136330 and neighbours); until now only isolated captures existed. Hypothesis until used by a validation script.
- **Jump**: `jump_obj` (`rider+0x5E40`) `+0x0C` = 1 while the button is charged, 2 on the release frame, 0 after; `+0x08` = 1 from press to ~0.1 s after landing. Input-device independent. The `rider_input+0x38` that `detect.py` read is a different button (3 presses in ALPINE, 0 in MOAB and ALPINET despite dozens of jumps); the jump press shows up in a different `rider_input` word per input source (ALPINEMX: `+0x28`, analog 0-255, mirrors `+0x54/+0x1B8/+0x1E4`). Charge times seen: 0.1-2.5 s.
- **Detector**: `jump_short`/`jump_long` read `+0x38` → never covered. They now read `jump_obj+0x0C`; short < 0.4 s, long ≥ 0.6 s (the 0.4-0.8 band used to count for neither). Replaying the 4 races through `Coach`: full script (before: jumps, pedal and time missing).
- `consistent: False` on 6- and 8-rider races: the roster (`0x7684A0`) always lists 10 entries; the check required equality with the rider count. Now `count <= entries`.
- Pedal: 10 s flat out in a row is not reachable (stamina); target is now 5 s cumulative; time 150 s.
