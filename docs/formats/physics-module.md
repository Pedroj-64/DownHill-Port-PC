# Physics module of the bike / módulo físico de la bici (hito 3)
Source: disassembly (ELF); every statement cites an address; unverified = **hypothesis**. Body layout (`module + 0x10`): `integrator.md`.

## EN — Module layout (`rider + 0x6420`), data first
| off | type | meaning | evidence |
|---:|---|---|---|
| 0x000 | ptr | class/vtable pointer (`0x28F6A8`) | `FUN_00133A58` 0x133A5C-0x133A64 |
| 0x004 | ptr | owner (rider/context object) | `FUN_00133A58` 0x133A84; used `owner+0x40C0`, `+0x42A0`, `+0x792C` |
| 0x008 | ptr | node; its u64 flags: bit 57 cleared during the step and restored after | `FUN_00133FA8` 0x133FF8-0x134050 |
| 0x010 | body | rigid body (0x1E0 − 0x10 bytes; fields in `integrator.md`) | `FUN_00133A58` → `FUN_00237928` |
| 0x110 | f32 | 100.0 (mass for the weight force) | `FUN_00133AB0` 0x133AB0; read 0x13409C |
| 0x114 / 0x118 | f32 | 0.2 / 0.28 (restitution / friction) | 0x133ABC / 0x133AD8 |
| 0x11C / 0x120 | f32 | 0.987 angular / 0.975 linear per-tick factor | 0x133AE8-0x133B38; read 0x134080/0x13408C |
| 0x124 | f32 | 3.0 (no reader found in the step path) | 0x133B28 |
| 0x128 / 0x12C / 0x130 | u32 | 180 / 4 (contact count) / 4 | 0x133B3C-0x133B44 |
| 0x150 + 16·i | vec4 | contact point i (x,y,z) and radius in w: (0,2.5,−2.6,.75) (0,−1.4,−2.8,.75) (0,1.5,0.5,.75) (0,−.75,0,.75) | 0x133B5C-0x133C28 (+ radius from `bike-physics.md`) |
| 0x1D4 / 0x1DC | u32 | step counters, +1 per `FUN_00133FA8` | 0x133FDC-0x133FF4 |
| 0x1D8 | u32 | handle returned by `FUN_0017F6E8` | `FUN_00133C38` |
| 0x1E0 | u8 | flag, cleared on init/reset | 0x133A90, 0x133C5C |
| 0x250 | vec4 | accumulated external impulse | `FUN_00133A10` |

## Entry points and call tree
- Constructor: `FUN_0011C710` → `FUN_0012B6C8` (0x12BC48) → `FUN_00133A58` → `FUN_00237928` (body init), `FUN_00133AB0` (parameters).
- Reset: `FUN_00133C38` (`+8 = 0`, flag 0). External impulse: `FUN_00133A10` (`FUN_00237CF0` + accumulate +0x250). Periodic push: `FUN_00133F28` (when `+0x1DC ≥ 30`: impulse = `owner+0x42A0 · gp[−0x7E10]`, counter reset).
- Per-tick step: `FUN_00133FA8` (caller `FUN_001586B8`, 0x1586F4; no direct caller of that one: vtable/indirect, **hypothesis**) → `FUN_00134060` (zero F/T, damp, gravity: see `integrator.md`) → `FUN_001340D8` (sub-step loop) → `FUN_00237A70(owner+0x40C0, body)`.
- Inside `FUN_001340D8`: contact points `FUN_001348A0` (0x134158, 0x1342AC, 0x1342F4), sweep `FUN_0021A908` (0x13430C), first hit `FUN_00217450` (0x13431C, 0x134454), depenetration `FUN_001344F0` (0x13436C), response `FUN_001344D0` (0x13441C). Other users of the sweep (not the bike): `FUN_00131C90`, `FUN_00131928`, `FUN_00146D70`, `FUN_00158B50`, `FUN_00167190`, `FUN_001A98D8`, `FUN_001AEF68`.
- Body functions (0x237xxx-0x238xxx): `FUN_00237960` (P, vel `*= k`), `FUN_00237998` (L, ω `*= k`), `FUN_00237C78` (F=T=0), `FUN_00237C88` (F += v), `FUN_00237CF0` (impulse), `FUN_00238818` (integrator).
- Shape/contact structures: shapes are the 4 sphere points above (no mesh); the collision "shape" side (BVH parts, `FUN_00219970`) is in `collision.md`. Contact response: see *Impact response* below.

## ES — Disposición del módulo (`rider + 0x6420`), datos primero
Misma tabla y direcciones que arriba: +0x000 puntero de clase (`0x28F6A8`); +0x004 propietario; +0x008 nodo (flags u64: el bit 57 se limpia durante el paso y se restaura); +0x010 cuerpo rígido (campos en `integrator.md`); +0x110 = 100,0 (masa para el peso); +0x114/+0x118 = 0,2 / 0,28 (restitución / fricción); +0x11C/+0x120 = 0,987 angular / 0,975 lineal (factor por tick); +0x124 = 3,0 (sin lector en la ruta del paso); +0x128/+0x12C/+0x130 = 180 / 4 (nº de contactos) / 4; +0x150 + 16·i = punto de contacto i (x,y,z) y radio en w; +0x1D4/+0x1DC contadores de paso; +0x1D8 asa de `FUN_0017F6E8`; +0x1E0 indicador; +0x250 impulso externo acumulado.
**Puntos de entrada.** Constructor: `FUN_0011C710` → `FUN_0012B6C8` → `FUN_00133A58` → `FUN_00237928` y `FUN_00133AB0`. Reinicio: `FUN_00133C38`. Impulso externo: `FUN_00133A10`. Empujón periódico: `FUN_00133F28`. Paso por tick: `FUN_00133FA8` (la llama `FUN_001586B8`; esta no tiene llamador directo: vtable/indirecto, **hipótesis**) → `FUN_00134060` → `FUN_001340D8` → `FUN_00237A70`. Dentro de `FUN_001340D8`: puntos `FUN_001348A0`, barrido `FUN_0021A908`, primer impacto `FUN_00217450`, despenetración `FUN_001344F0`, respuesta `FUN_001344D0`. Respuesta de contacto: ver *Respuesta a impactos* abajo.

## EN — Impact response (mapped 2026-10-04 from the disassembly), data first
**Hit record** (0x30 B, produced by the sweep; `a1` of `FUN_00134630`): `+0x00` u64 with packed fields (material index = `(u64 >> 21) & 0xFFFF & 0x3F`, used to pick the dust/sound effect; other bits **hypothesis**), `+0x10` vec4 contact point (world), `+0x20` vec4 surface normal (world), unit length (0x134828-0x13483C).
**Module fields used:** `+0x114` restitution e (0.2), `+0x118` friction µ (0.28; ×1.25 when the point index is 2, 0x134660-0x134674), `+0x1D0` counter reset to 0 after an effect fires, global u32 at `gp − 0x7E28` = 0x2C5348 (cool-down threshold for `+0x1D0`; value not read), owner `+0x7720` (sound sink), `+0x792C` (effects block; byte `+5` = enabled), `+0x7A44`.
**Functions.**
- `FUN_001344D0(module, hit, pointIndex)`: tail call of `FUN_00134630`; the result is the "responded" flag of the sub-step loop (always 1, 0x134878).
- `FUN_00134630(module, hit, pointIndex)` (body = `module + 0x10`, node = `*(module + 8)`):
  1. `J_f = FUN_00238648(body, out, v_surf = 0, hit.point, hit.normal)`; `J_f.xyz *= µ`.
  2. `t = hit.normal × node.row0` (row 0 = lateral axis, node `+0x40`; 0x1346B8-0x1346D0), normalised with `rsqrt`; **if `|t|² > 0.5`** (before normalising): `J_f −= t·(J_f·t)` (anisotropic friction: the component along `t`, the rolling direction, is removed).
  3. `FUN_00237D28(body, J_f, hit.point)`.
  4. `J_n = FUN_002384B8(body, …, hit.point, hit.normal, e)`, then `FUN_00237D28(body, J_n, hit.point)` (the normal impulse uses the velocity already changed by the friction impulse).
  5. `|J_n| > 400.0` and `+0x1D0 ≥ gp[−0x7E28]`: `FUN_001D03B0(owner+0x7720, |J_n|)`, `+0x1D0 = 0`, then if the effects byte is set `FUN_001B9A50(…, material, |J_n| × 5.0e-4 (0x3A03126F), …)`. Cosmetic: **not part of the dynamics**.
- `FUN_00238648(body, out, v_surf, point, normal)`: `r = point − pos(+0x50)`; `v = vel(+0xC0) + ω(+0xD0) × r − v_surf`; `vn = n·v`; if `vn > −0.001` (0xBA83126F) → `out = (0,0,0,1)` (separating); else `t = v − n·vn`, `|t|`, `t̂ = t/|t|` (**no guard** for `|t| = 0`), `K = invMass + t̂·((I⁻¹(r × t̂)) × r)` (`I⁻¹` = body `+0x80` via `FUN_002275F0`), `out.w = |t|/K`, `out.xyz = −t̂·|t|/K`: the impulse that cancels the tangential velocity (**sticking**, not clamped by the normal impulse: the Coulomb limit is replaced by the factor µ in step 1).
- `FUN_002384B8(body, out, v_surf, point, normal, e)`: same `r`, `v`, `vn`; separating test identical; `K = invMass + n·((I⁻¹(r × n)) × r)`; `j = −(e + 1)·vn / K`; `out.w = j`, `out.xyz = n·j`.
- `FUN_00237D28(body, J, point)`: `FUN_00237CF0(body, J)` (P += J, vel), then `L += (point − pos) × J` and `ω = I⁻¹·L` (`FUN_002275F0`).
- Native port: `src/contact.hpp` `contactResponse` (friction, anisotropy, normal impulse; effects not ported). **Validation against captures: pending (impact capture).**

## ES — Respuesta a impactos (mapeada el 2026-10-04 desde el desensamblado), datos primero
**Registro de impacto** (0x30 B, lo produce el barrido; `a1` de `FUN_00134630`): `+0x00` u64 con campos empaquetados (índice de material = `(u64 >> 21) & 0xFFFF & 0x3F`, usado para elegir el efecto de polvo/sonido; el resto de bits **hipótesis**), `+0x10` vec4 punto de contacto (mundo), `+0x20` vec4 normal de la superficie (mundo), de longitud 1 (0x134828-0x13483C).
**Campos del módulo usados:** `+0x114` restitución e (0,2), `+0x118` fricción µ (0,28; ×1,25 si el índice del punto es 2, 0x134660-0x134674), `+0x1D0` contador que se pone a 0 al lanzar un efecto, u32 global en `gp − 0x7E28` = 0x2C5348 (umbral de enfriamiento de `+0x1D0`; valor no leído), propietario `+0x7720` (destino del sonido), `+0x792C` (bloque de efectos; byte `+5` = activo), `+0x7A44`.
**Funciones.**
- `FUN_001344D0(módulo, impacto, índicePunto)`: llamada final a `FUN_00134630`; su resultado es la bandera «respondió» del bucle de sub-pasos (siempre 1, 0x134878).
- `FUN_00134630(módulo, impacto, índicePunto)` (cuerpo = `módulo + 0x10`, nodo = `*(módulo + 8)`):
  1. `J_f = FUN_00238648(cuerpo, out, v_sup = 0, impacto.punto, impacto.normal)`; `J_f.xyz *= µ`.
  2. `t = impacto.normal × nodo.fila0` (fila 0 = eje lateral, nodo `+0x40`; 0x1346B8-0x1346D0), normalizado con `rsqrt`; **si `|t|² > 0,5`** (antes de normalizar): `J_f −= t·(J_f·t)` (fricción anisótropa: se quita la componente a lo largo de `t`, la dirección de rodadura).
  3. `FUN_00237D28(cuerpo, J_f, impacto.punto)`.
  4. `J_n = FUN_002384B8(cuerpo, …, impacto.punto, impacto.normal, e)`, luego `FUN_00237D28(cuerpo, J_n, impacto.punto)` (el impulso normal usa la velocidad ya modificada por el de fricción).
  5. `|J_n| > 400,0` y `+0x1D0 ≥ gp[−0x7E28]`: `FUN_001D03B0(propietario+0x7720, |J_n|)`, `+0x1D0 = 0` y, si el byte de efectos está activo, `FUN_001B9A50(…, material, |J_n| × 5,0e-4 (0x3A03126F), …)`. Cosmético: **no es dinámica**.
- `FUN_00238648(cuerpo, out, v_sup, punto, normal)`: `r = punto − pos(+0x50)`; `v = vel(+0xC0) + ω(+0xD0) × r − v_sup`; `vn = n·v`; si `vn > −0,001` (0xBA83126F) → `out = (0,0,0,1)` (se separa); si no, `t = v − n·vn`, `|t|`, `t̂ = t/|t|` (**sin guarda** para `|t| = 0`), `K = invMasa + t̂·((I⁻¹(r × t̂)) × r)` (`I⁻¹` = cuerpo `+0x80` vía `FUN_002275F0`), `out.w = |t|/K`, `out.xyz = −t̂·|t|/K`: el impulso que anula la velocidad tangencial (**adherencia**, sin limitar por el impulso normal: el límite de Coulomb se sustituye por el factor µ del paso 1).
- `FUN_002384B8(cuerpo, out, v_sup, punto, normal, e)`: mismos `r`, `v`, `vn` y prueba de separación; `K = invMasa + n·((I⁻¹(r × n)) × r)`; `j = −(e + 1)·vn / K`; `out.w = j`, `out.xyz = n·j`.
- `FUN_00237D28(cuerpo, J, punto)`: `FUN_00237CF0(cuerpo, J)` (P += J, vel), luego `L += (punto − pos) × J` y `ω = I⁻¹·L` (`FUN_002275F0`).
- Port nativo: `src/contact.hpp` `contactResponse` (fricción, anisotropía, impulso normal; efectos no portados). **Validación contra capturas: pendiente (captura de impacto).**
