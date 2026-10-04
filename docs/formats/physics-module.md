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
- Shape/contact structures: shapes are the 4 sphere points above (no mesh); the collision "shape" side (BVH parts, `FUN_00219970`) is in `collision.md`. Contact response (`FUN_00134630` → `FUN_00238648/002384B8/00237D28`) is **not mapped yet**.

## ES — Disposición del módulo (`rider + 0x6420`), datos primero
Misma tabla y direcciones que arriba: +0x000 puntero de clase (`0x28F6A8`); +0x004 propietario; +0x008 nodo (flags u64: el bit 57 se limpia durante el paso y se restaura); +0x010 cuerpo rígido (campos en `integrator.md`); +0x110 = 100,0 (masa para el peso); +0x114/+0x118 = 0,2 / 0,28 (restitución / fricción); +0x11C/+0x120 = 0,987 angular / 0,975 lineal (factor por tick); +0x124 = 3,0 (sin lector en la ruta del paso); +0x128/+0x12C/+0x130 = 180 / 4 (nº de contactos) / 4; +0x150 + 16·i = punto de contacto i (x,y,z) y radio en w; +0x1D4/+0x1DC contadores de paso; +0x1D8 asa de `FUN_0017F6E8`; +0x1E0 indicador; +0x250 impulso externo acumulado.
**Puntos de entrada.** Constructor: `FUN_0011C710` → `FUN_0012B6C8` → `FUN_00133A58` → `FUN_00237928` y `FUN_00133AB0`. Reinicio: `FUN_00133C38`. Impulso externo: `FUN_00133A10`. Empujón periódico: `FUN_00133F28`. Paso por tick: `FUN_00133FA8` (la llama `FUN_001586B8`; esta no tiene llamador directo: vtable/indirecto, **hipótesis**) → `FUN_00134060` → `FUN_001340D8` → `FUN_00237A70`. Dentro de `FUN_001340D8`: puntos `FUN_001348A0`, barrido `FUN_0021A908`, primer impacto `FUN_00217450`, despenetración `FUN_001344F0`, respuesta `FUN_001344D0`. Respuesta de contacto (`FUN_00134630` → `FUN_00238648/002384B8/00237D28`): **sin mapear aún**.
