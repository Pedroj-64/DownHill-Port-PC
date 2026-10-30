# Level names vs. courses and race types / Nombres de nivel frente a cursos y tipos de carrera

Evidence / Evidencia: the 51 level names on the disc (`tools/sweep_levels.py` lists them) compared with the community wiki's course list. Name-based inference, not read from the executable: treat the "inferred" rows as hypotheses.

## EN
The wiki lists 9 courses (7 mountains, a canyon, an urban track) with up to three race types each: Freeride (FR), Technical Downhill (TD) and Mountain Cross (MX), plus bonus levels. The disc has 51 levels, which fit as **course × race type × 2 variants**, plus the bonus and tutorial levels:

| Prefix | Course (wiki) | Basis | Levels |
|---|---|---|---|
| `ALP` | Mt. MiDoule (Alps, Italy) | name | `ALPINE ALP2` (FR) · `ALPINET ALPT2` (TD) · `ALPINEMX ALPX2` (MX) |
| `MOAB` | Red Pass (Utah) | name (Moab) | `MOAB MOAB2` · `MOABTDH MOABTDH2` · `MOABMX MOABMX2` |
| `FUJI` | Mt. Imasaki (Japan) | name; the wiki says Freeride only, and there are only `FUJI FUJI2` | `FUJI FUJI2` |
| `PERU` | Mt. Quintanaroo (Peru) | name | `PERU PERU2` · `PERUTDH PERUTDH2` · `PERUMX PERUMX2` |
| `CITY` | Salt City | name | `CITY CITY2` · `CITYTDH` · `CITYMX` (no second variant seen) |
| `BC` | Mt. McSchley (British Columbia) | name; wiki: FR and MX only | `BC BC2` · `BCTDX BCTDX2` |
| `GLAC` | Mt. Zorkovaska (Siberia) | inferred (glacier) | `GLACIER GLAC2P` · `GLACIERT GLACT2P` · `GLACIERX GLACX2P` |
| `JUNGLE` | Mt. Konawaki (Hawaii) | inferred (by elimination) | `JUNGLE JUNGLE2` · `JUNGLETD JUNGLET2` · `JUNGLEMX JUNGLEX2` |
| `AUBER` | Mt. Liddellroch (Scotland) | inferred (by elimination) | `AUBERST AUBERST2` · `AUBERSTD AUBTDH2` · `AUBERMX AUBERMX2` |
| `DUAL` `MOSH` `MONKEY` | bonus: Dual Slalom, Moshbowl, Super Jump | inferred (`MONKEY` as Super Jump is a guess) | `DUAL` · `MOSH MOSH2` · `MONKEY` |
| `TRAINER` | Training | inferred | `TRAINER` |

Suffix reading: `MX` = Mountain Cross, `T`/`TD`/`TDH` = Technical Downhill, no suffix (or `ST`) = Freeride, `2`/`2P` = second variant. The sweep shows the two variants have the same or almost the same geometry metrics (`ALP2` equals `ALPINE`; `FUJI`/`FUJI2` differ by 4 chunks), so what the variant changes (time of day, direction, arcade layout) is **unknown**.

Things that do not fit and need a check: `BCTDX` is named like Technical Downhill but the wiki says McSchley has no TD (so it may be its Mountain Cross); `AUBERST` against `AUBERSTD`; the Konawaki/Liddellroch assignment; whether `MONKEY` is Super Jump. None of this was tested in the emulator.

## ES
La wiki lista 9 cursos (7 montañas, un cañón y una pista urbana) con hasta tres tipos de carrera: Freeride (FR), Technical Downhill (TD) y Mountain Cross (MX), además de los niveles bonus. El disco tiene 51 niveles, que encajan como **curso × tipo de carrera × 2 variantes**, más los niveles bonus y el de entrenamiento. La tabla de arriba da la correspondencia por prefijo.

Lectura de sufijos: `MX` = Mountain Cross, `T`/`TD`/`TDH` = Technical Downhill, sin sufijo (o `ST`) = Freeride, `2`/`2P` = segunda variante. El barrido muestra que las dos variantes tienen métricas de geometría iguales o casi iguales (`ALP2` igual a `ALPINE`; `FUJI` y `FUJI2` difieren en 4 chunks), así que **no se sabe** qué cambia la variante (hora del día, sentido, disposición de arcade).

Lo que no encaja y hay que comprobar: `BCTDX` se llama como Technical Downhill pero la wiki dice que McSchley no tiene TD (puede ser su Mountain Cross); `AUBERST` frente a `AUBERSTD`; la asignación de Konawaki y Liddellroch (por eliminación); si `MONKEY` es Super Jump. Nada de esto se probó en el emulador.
