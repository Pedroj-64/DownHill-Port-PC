# Rigid-body integrator of the bike / integrador del cuerpo rígido (hito 3, carril B)
Port: `src/integrator_fidel.hpp` · Tests: `tests/integrator_test.cpp` (synthetic) · Harness with real states: `tools/integrator_check.py` · Op catalogue: `docs/formats/vu0-macro-ops.md`.
Source: the **disassembly** (the decompiler loses the COP2/MMI ops). Rule: every statement cites an address; unverified = **hypothesis**.

## Body layout (base = physics module + 0x10; `FUN_001340D8` passes `iVar17 + 0x10`)
| off | type | meaning | evidence |
|---:|---|---|---|
| 0x00 | f32 | inverse mass (0.01 in all bikes: m = 100) | `FUN_002389B8` 0x2389e0/f0, `FUN_00237CF0` 0x237d04; `vel = P·this` holds to 5e-7 in 16 riders |
| 0x10 | vec4 | local centre-of-mass offset (0, 0, −0.85) | `FUN_00238818` 0x238960 |
| 0x20 | f32×3 | diagonal of the inverse inertia, in 0x20,0x24,0x28 (0.0023, 0.0047, 0.0038 …) | `FUN_002389B8` 0x2389b8-0x238a48 |
| 0x30 | ptr | float slot receiving the last `dt` | `FUN_00238818` 0x238890 |
| 0x34 | ptr | node position (vec4) | `FUN_00238818` 0x23885c, 0x238980 |
| 0x38 | ptr | node matrix, 4×4, rows = axes | 0x23886c, `FUN_002389B8` 0x2389dc |
| 0x3C / 0x40 | ptr | previous-pose copies (position / 4×4); only used if both exist | 0x238844-0x23888c |
| 0x50 | vec4 | centre-of-mass position | 0x2388c4 |
| 0x60 | vec4 | linear momentum P (w = 1) | 0x238908; `P/vel = 100` |
| 0x70 | vec4 | angular momentum L (w = 1) | 0x23892c |
| 0x80 | 4×4 | "world inverse inertia" **(measured: always diagonal = 0x20)** | `FUN_002277C8` result, see below |
| 0xC0 | vec4 | velocity = P·invMass | `FUN_002389B8` 0x2389f0 |
| 0xD0 | vec4 | angular velocity = (0x80)·L | `FUN_002389B8` → `FUN_002275F0` |
| 0xE0 / 0xF0 | vec4 | force / torque accumulators, **zeroed to (0,0,0,1) after each integration** | `FUN_00237C78` |

## Functions traced (all from disassembly)
| Function | What it computes |
|---|---|
| `FUN_00238818(body, dt)` | **the integrator**; order: (0) `dt ≤ 0` → return; (1) if `*(+0x3C)` and `*(+0x40)` exist: copy node position and 4×4, `*(+0x30) = dt`; (2) `S = [ω]×` (`FUN_00238B70` with ω = +0xD0); `S ← S·Rᵀ` (`FUN_00227820`, R = old matrix); (3) `pos += dt·vel` (old velocity); (4) `S *= dt` (`FUN_00227988`); `R[i][j] += S[j][i]` (`FUN_002278E8`) = `R[i] += dt·(ω × R[i])`; `orthonormalize(R)` (`FUN_00228568`); (5) `P += dt·F`, `L += dt·T`; (6) `FUN_002389B8`: vel, iw, ω from the new momenta; (7) `node = pos − com·R` (0x238960-0x238994). Explicit Euler with the **old** velocity and angular velocity |
| `FUN_00238B70(body, out, ω)` | skew-symmetric matrix `S` (`S·v = ω × v`): out[0][1] = −ωz, [0][2] = ωy, [1][0] = ωz, [1][2] = −ωx, [2][0] = −ωy, [2][1] = ωx, diagonal 0 (0x238b70-0x238bc0) |
| `FUN_00227820(out, in, A)` | rows 0..2 of `in`: `v ← A·v`; row 3 unchanged (PMMI transpose, see vu0-macro-ops.md) |
| `FUN_00227988(out, in, k)` | rows 0..2 `.xyz *= k` (writes only 3 rows) |
| `FUN_002278E8(out, a, b)` | `out[i][j] = a[i][j] + b[j][i]` (3×3, writes only the 9 floats) |
| `FUN_00228568(R)` | re-orthonormalisation: `r0 = N(r1×r2)`, `r1 = N(r2×r0)`, `r2 = N(r0×r1)` (`vopmula/vopmsub` + `rsqrt.S`); measured `R·Rᵀ − I < 4e-7` in 16 riders |
| `FUN_002389B8(body)` | `vel = P·invMass`; identity via `FUN_00227C88`; `sp[i][j] = d_i·R[j][i]`; `iw = sp·R` (`FUN_002277C8`); `ω = iw·L` (`FUN_002275F0`) |
| `FUN_002275F0(A, v, out)` | `out = A·v` (row·v dot products); argument types were lost by the decompiler: confirmed |
| `FUN_00237CF0(body, J)` | **impulse**: `P += J` (xyz) and `vel = P·invMass` (arguments now confirmed: (body, J)) |
| `FUN_00237AB0(body, d)` | translate: node position += d and +0x50 += d (used by the depenetration `FUN_001344F0`) |
| `FUN_00237C78(body)` | `F = T = (0,0,0,1)` |
| `FUN_00238BC8(body, buf)` / `FUN_00238C60(body, buf)` | save / restore 0xE0 bytes: node position (0x00), node 4×4 (0x10), +0x50, +0x60, +0x70, +0x80 (4×4), +0xC0, +0xD0. **Force and torque are not part of the snapshot** |
| `FUN_0023F078()` | returns the u32 at `gp − 0x4B74` = **0x2C85FC** (gp = `0x2CD170` from `entry` 0x10A148 `lui a0,0x2d` / 0x10A15C `addiu a0,a0,-0x2e90` / 0x10A170 `move gp,a0`) = **50** in every savestate: the physics step is `dt = 1/50 s` (PAL 50 Hz). 728 samples in 14.5 s of game time in `docs/p2s-savestates.md` (live PINE capture) = 50.2 Hz agree |

## Sub-step loop of `FUN_001340D8` (once per physics step)
`dtTick = 1/50`; `rem = 1`; at most 2 iterations. Each: save the body (`FUN_00238BC8`), compute the contact points at the start pose (`FUN_001348A0`), **integrate the remaining step** `FUN_00238818(dtTick·rem)`, zero force/torque (`FUN_00237C78`), compute the end points, sweep (`FUN_0021A908`), take the first hit (`FUN_00217450`). No hit → done. If this was the 2nd iteration the hit fraction is forced to −1 *without restoring* (→ depenetration); otherwise restore (`FUN_00238C60`). `frac < 0` → depenetrate (`FUN_001344F0`). Else, for each hit in order: integrate `dtTick·rem·frac` (force is already 0 here), call the response `FUN_001344D0(module, hit, point)`; if it responds: `rem −= rem·frac` (the decompiler shows `(int)f * (f >= 0)`, read as a clamp to ≥ 0 — **hypothesis**) and continue with the next iteration; if no hit responds: restore and integrate the full remaining step. Ported as `integ::tick(...)` with callbacks (sweep / depenetration / response), no geometry inside.

## What is validated, and how (savestates in `~/.var/app/net.pcsx2.PCSX2/config/PCSX2/sstates/`, never in the repo)
`tools/integrator_check.py estado.p2s` on 16 riders in 2 distinct states (slot 01 = ALPINEMX, slot 05 = ALP2):
| check | result |
|---|---|
| `vel = P·invMass` | max error 4e-6 (float) |
| `ω = iw·L` and `iw = diag(0x20)` | 5e-7 and 1e-9; **`ω = Rᵀ D R·L` (rotated inertia) is rejected: error 0.1 … 3.5** |
| `R·Rᵀ = I` | < 4e-7 (re-orthonormalisation active) |
| `dt` divisor | 50 (u32 at 0x2C85FC, all states) |
**Finding:** `iw = sp·R = D·Rᵀ·R = D` for any orthonormal `R`: the engine integrates the angular velocity with the **body-frame diagonal inertia applied to L without rotating it** (ω = D·L, L being the stored vector). Faithfully ported (`updateDerived`).
**Not validated (needs consecutive states):** the position/orientation update itself (`pos += dt·vel`, `R[i] += dt·ω×R[i]`, the PMMI transpose direction — chosen because it is the physically consistent one: the other direction would rotate the opposite way), the force/torque magnitudes, the sub-step loop.
**Discrepancy (open):** the tail `node = pos − com·R` does **not** hold in the saved states: `|node − (pos − com·R)|` = 560 … 10 000 in every rider. The momenta/velocities are consistent, the stored `+0x50` is not tied to the node. Slots 05 and 10 (same race, node moved 25 u, HUD 1 s apart) have **identical** body fields (`tools/integrator_check.py 05 10`: displacement 0 in all riders; only 8 bytes of the module differ). **Hypothesis:** in these snapshots the player/AI nodes are driven by something other than the physics (replay/ghost or scripted intro) or the body is only live in other game states; nothing here confirms that the module integrated in these states. Slots 01–04 and 06–09 are byte-for-byte the same state (an earlier `pine_capture.py` run overwrote slots 2–9, per `docs/p2s-savestates.md`).

## How to capture the pair I need (no invented data)
Goal: states **exactly 1 physics step apart, bike in the air without any ground contact** (so there is no collision response), to compare `pos`, `R`, `P`, `L`.
**A. Manual (PCSX2 Qt, Flatpak):** 1) load a race state with the player airborne (e.g. the long drop in ALP2); 2) `Settings → Hotkeys → System`: check the keys of *Pause*, *Frame Advance*, *Save State*, *Next Save Slot* (version-dependent defaults; typical: Pause = Space, Save = F1, Next slot = F2); assign *Frame Advance* if it is empty; 3) pause, save to slot X; 4) frame-advance **once**, change slot, save; repeat to get 4–5 consecutive states. Copy them out of the folder (or tell me their slot numbers); **do not** run `pine_capture.py` meanwhile, it overwrites slots 2–9.
**B. Automatic (PINE, 50 consecutive steps per second):** back up the folder first (`cp -r …/sstates /tmp/sstates.bak`), close PCSX2, then `python3 tools/pcsx2/pine_capture.py --iso <iso> --slot <airborne state> --seconds 10 --out /tmp/dhcap/air.cap` and `python3 tools/integrator_check.py /tmp/dhcap/air.cap`. It stores, per physics step, the player's node and the 0x1E0 bytes of the physics module (all the fields above); it overwrites slots 2–9.
**What I will measure:** `|pos err|` (expected ~1e-5 if the update is as ported), `|R err|`, implied `F = (P1−P0)/dt` (airborne: only gravity, `F = m·g`, so `F_z ≈ −100·32.17 = −3217`, expected from the earlier 1 u = 1 ft finding), implied `T`, and whether `node = pos − com·R` holds on a live body.
