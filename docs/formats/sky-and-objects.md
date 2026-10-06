# Cielos y objetos de nodo (fase 4) / Skies and node objects (phase 4)
Estado 2026-10-06. Solo números y comparaciones; capturas fuera del repo (`~/dh-captures/`). Todo lo no marcado «verificado» es **hipótesis**. / Status 2026-10-06. Numbers and comparisons only; screenshots outside the repo. Anything not marked "verified" is a **hypothesis**.

## ES — Cielo de los 7 niveles sin domo
Regla (`tools/extract_model.py`): raíz del grafo con ≤ 16 dueños, sin celdas de terreno (capa 3), hojas con traslación < 1500 y radio mayor que un mínimo. El mínimo era 3000; ahora se prueba 3000 y, **solo si ninguna raíz cumple**, 2000.
| Nivel | Raíces candidatas | Resultado |
|---|---|---|
| `PODARC`, `PODSUP` | 1 raíz (`0x10`), 1 hoja, **10 326 triángulos**, radio 2360, centrada en el origen | **domo de cielo** (degradado azul con nubes y anillo de montañas, visto en el visor); antes sin cielo |
| `PODSPE` | ya tiene domo (radio 5000, 270 triángulos) y un segundo objeto de radio 2360 | **sin cambios** (con 2000 incondicional el segundo objeto solo añadía ruido) |
| `AUBERMX`, `AUBERMX2`, `MOSH`, `MOSH2`, `TRAINER` | ninguna: sus raíces son la escena (1667 / 210 / 94 hojas), objetos pequeños (radio ≤ 69) y 1-2 hojas de radio 45 | **sin geometría de cielo en los datos**; el azul pálido del visor es su color de fondo por defecto |
El barrido de los 54 niveles con ambos umbrales cambia exactamente 3 (`PODARC`, `PODSUP`, `PODSPE`); con la alternativa solo cambian `PODARC` y `PODSUP`. **Abierto:** el fondo real de los 5 niveles sin domo (¿color de limpieza de pantalla por nivel?). Sin fotogramas del juego de esos niveles no se puede contrastar; hace falta un savestate de cada uno (`AUBERMX`, `MOSH`, `TRAINER`).

## ES — Comparación con el juego (`tools/compare_view.py`)
- **ALP2** (estado 05) y **ALPINEMX** (estado 01): terreno, árboles, cabaña, carteles de rombo, banderolas y montañas coinciden con el fotograma real; el cielo violeta de ALP2 también (domo correcto). **Faltan** en el modelo: el piloto, los **conos naranjas** y los **espectadores** de la zona de salida (figuras azules/cian) y las siluetas de pájaros.
## ES — Catálogo de nodos tipo 25 / 11 (54 niveles, alcanzables desde la raíz)
- **Tipo 11** (marcadores): `kind 8050` ×345 (29 niveles), `8051` ×256 (47), **`8052` ×47 (uno por nivel en 47 de 54: la meta, hipótesis)**; `1629/1630/1631` ×247/348/515 (32-34 niveles, los más comunes; sin callback = escenografía); resto 1600-1704 y 7748/7795/7799, 13100-13114 (uno por nivel).
- **Tipo 25** (contenedores de malla): `4000-4020` ×374 cada uno (34 niveles), `4030-4130` = cuerpos de pilotos (ver `rider.md`), `1200-1491` (props: 18 familias, 2-18 niveles), y una familia presente con los mismos recuentos en ALP2 y ALPINEMX: `8070` ×1, `8120` ×1, `8121` ×9, `8130` ×2, `8131` ×9, `8140` ×2, `8141` ×9, `8600` ×1, `13000` ×6 — candidatos a plataformas de salida, meta y objetos de la zona (hipótesis; por trazar con `FUN_00177458` y `FUN_00195b80`).
- No se ha identificado qué nodo es la plataforma de salida/meta ni dónde están los conos y espectadores (no están entre los nodos tipo 25 de `ALPINEMX` en `4000-4020`, que ese nivel no tiene).

## EN — Sky for the 7 levels without a dome
Rule (`tools/extract_model.py`): a graph root with ≤ 16 owners, no terrain cells (layer 3), leaves with translation < 1500 and a radius above a minimum. The minimum was 3000; now 3000 is tried and, **only if no root qualifies**, 2000.
| Level | Candidate roots | Result |
|---|---|---|
| `PODARC`, `PODSUP` | 1 root (`0x10`), 1 leaf, **10,326 triangles**, radius 2360, centred on the origin | **sky dome** (blue gradient with clouds and a mountain ring, seen in the viewer); previously no sky |
| `PODSPE` | already has a dome (radius 5000, 270 triangles) and a second radius-2360 object | **unchanged** (an unconditional 2000 only added noise) |
| `AUBERMX`, `AUBERMX2`, `MOSH`, `MOSH2`, `TRAINER` | none: their roots are the scene (1667 / 210 / 94 leaves), small objects (radius ≤ 69) and 1-2 radius-45 leaves | **no sky geometry in the data**; the pale blue in the viewer is its default background colour |
Sweeping the 54 levels with both thresholds changes exactly 3 (`PODARC`, `PODSUP`, `PODSPE`); with the fallback only `PODARC` and `PODSUP` change. **Open:** the real background of the 5 dome-less levels (a per-level screen-clear colour?). Without game frames of those levels it cannot be checked; a savestate of each (`AUBERMX`, `MOSH`, `TRAINER`) is needed.

## EN — Comparison with the game (`tools/compare_view.py`)
- **ALP2** (state 05) and **ALPINEMX** (state 01): terrain, trees, cabin, diamond signs, banners and mountains match the real frame; ALP2's purple sky too (correct dome). **Missing** in the model: the rider, the **orange cones** and the **spectators** at the start area (blue/cyan figures) and the bird silhouettes.
## EN — Type 25 / 11 node catalogue (54 levels, reachable from the root)
- **Type 11** (markers): `kind 8050` ×345 (29 levels), `8051` ×256 (47), **`8052` ×47 (one per level in 47 of 54: the finish, hypothesis)**; `1629/1630/1631` ×247/348/515 (32-34 levels, the commonest; no callback = scenery); the rest 1600-1704 and 7748/7795/7799, 13100-13114 (one per level).
- **Type 25** (mesh containers): `4000-4020` ×374 each (34 levels), `4030-4130` = rider bodies (see `rider.md`), `1200-1491` (props: 18 families, 2-18 levels), and a family with identical counts in ALP2 and ALPINEMX: `8070` ×1, `8120` ×1, `8121` ×9, `8130` ×2, `8131` ×9, `8140` ×2, `8141` ×9, `8600` ×1, `13000` ×6 — candidates for start platforms, finish and area objects (hypothesis; to trace with `FUN_00177458` and `FUN_00195b80`).
- It is not known which node is the start/finish platform nor where the cones and spectators are (they are not among `ALPINEMX`'s type-25 nodes of `4000-4020`, which that level lacks).
