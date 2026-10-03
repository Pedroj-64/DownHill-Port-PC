# Rider body pose in RAM: search log (2026-10-02) / pose viva del cuerpo del piloto: registro de búsqueda
Tool: `tools/rider_live.py` (probes over an `eeMemory.bin`; dumps never go into the repo). **Result: the 40-channel body pose array was NOT located** in savestates 01 (ALPINEMX) and 05 (ALP2-like). Everything below is evidence for the next attempt. Uncited = hypothesis.

## Walker and pose parameter (ELF)
* `FUN_0020edb8(node, callback, pos)` (0x20edb8, 328 B + the jump-table body at 0x20effc) is the generic scene walker; callers `FUN_001595e8` (per-scene, callback `0x159910` = kind dispatcher) and `FUN_001597a8`. It keeps an explicit stack of 0x60-byte entries at `0x501020` (matrix 0x50 B, `+0x50` child cursor, `+0x54` remaining, `+0x58` **pose struct**, `+0x5c` depth). The root entry is pushed with `+0x58 = 0` (`FUN_0020edb8`: `*(0x501078 + n*0x60) = 0`) and every later push just copies it (`pfVar11[0x16] = fStack_138`); **no case of the walker ever stores a non-zero pose**. So case `0x11` (joint table, 0x20f4e8: reads `pose[+4][channel]`, writes the Euler matrix into `*(pose+0xc) + table[+0xc4]*0x40`) runs, in the static level walk, with pose = 0: `*(int*)(0+4)` / `*(int*)(0+0xc)` read EE addresses 0x4/0xc (valid RAM on the PS2, garbage values).
* Evidence it did run that way: in RAM the resident joint tables have the channel-presence mask in byte `+0xc1` (0x07 / 0x01 / 0x3f) where the file has 0x03 / 0x01 / 0x1f, and the Euler fields `+0x20..0x28` are 0 for all 15 joints of ALP2 kind 4090 (st05).
* `FUN_0021ac18` / `FUN_0021ad00` (the second implementation of case 0x11, with `flags & 1` root translation, see rider.md) are reached only from `FUN_0021aea8` / `FUN_0021b1b0` → `FUN_00217f48` / `FUN_00218918`, i.e. the **collision hierarchy** (same joint tables used by dynamic collision objects; `FUN_0021a908` callers are the sweep code), not from the rider draw.
* Rider objects: callback `FUN_00159910` → `FUN_00195b80` (kind − 4000 ≡ 0 mod 5 → `FUN_001955a8` + `FUN_00195fb8`, 0x94-byte objects at `0x41f9a0`; other kinds → `FUN_001957a8`). `FUN_001955a8(obj, node)`: `obj+0x248 = header>>7 & 0x7ff`, `obj+4 = FUN_0020e758()` (pooled model instance), `FUN_0021dfe0(inst, node)` (`inst+0 = node`, `inst+8 = node+0x1c` if type 1), `obj+8` = a second instance. In st05 these objects (vtable `0x293118`, stride `0x250`) point at instances such as `0x7aa8c0` that hold a **world position + 3×3 rotation + scale 50/1.4 and a copy of the node header** → they are placed scenery/spectator objects (19 of them), not the 10 racers.

## What the RAM contains
* **Pose arrays**: the only 40+-channel float arrays found are the shared bike pose (`0x7ed430`, st05) = BIKESKEL/BANIM (rider-mount.md). Anim players (type 0x21) hold `+4 = pose, +8 = mask (= pose + 4·nchan), +0xc = matrices`; distinct matrix arrays seen: `0x15d2c60, 0x160c680, 0x1641e40, 0x16771a0, 0x16afe60, …` (the resident first-person-hands rigs `R/*`), `0x15f3810` (BIKESKEL).
* **Joint matrices of the body rig**: a scan for 4th rows equal to the rig's `loc` vectors (shoulder (0.54, −0.077, 0.521, 1), hip (−0.369, −0.083, −0.415, 1), knee (0, 0.121, −1.385, 1), head (0, 0, 0.9, 1)) finds only the NGP tables themselves (86 skeleton copies in the resident level image at `0x13b…–0x141…` for st05) — **nothing outside the image**. Either the body palette is built transiently and not present at the snapshot instant, or it is not built with that code path.
* A 21-matrix orthonormal run per rider at `0x77bff0 + 0x680·i` (rotation-only, row 3 = (0,0,0,1)) does **not** decode as the joint tree's local/world rotations (hinge test: `R_elbow·R_shoulder⁻¹` is not a Z rotation, knee not X, for any window of 15 in the whole RAM); probably rigid-body/ragdoll data.
* Brute-force scan of all 40-float windows with plausible hinge channels (15/22, 29/36 non-trivial, |v| < 3.5): 196 161 candidates; a wrist-separation/ankle-separation score (≈ 2.06 / 0.98 u from the bike anchors) is too weak — it is dominated by near-zero arrays and matrix data. Not conclusive.

## Hypotheses worth testing
1. The body pose may never exist as a float array: the draw path could sample `R/RANIM`/`SHRANIM` curves (channels 0–39 exist there) straight into the palette, or blend them with an IK result at draw time.
2. The skeleton evaluation for riders may run with a pose struct created at spawn (`FUN_00195fb8`/`FUN_00196090` register the object in `0x77a390`/`0x781db8` per-scene lists: `FUN_00196090(obj) = (obj − 0x41f9a0)·0x… >> 4` = object index) and be called from the per-frame rider update, not from `FUN_0020edb8`.

## What is needed from PCSX2 (GUI debugger, not scriptable from here)
**A write breakpoint on the Euler field of a resident joint table**, e.g. ALP2 kind 4090 root table: RAM `0xA00000 + 0x9e2e60 + 0x20 = 0x13e2e80` (st05 has ALP2-sized resident NGP at `0xA00000`; confirm with the first bytes of the table: `11 00 00 00 03 00 00 00` at `0x13e2e60`). Break on write while the game runs a race, then read: the **PC** (the writer), `$s0/$s2` or the register holding the pose pointer (`pose + 4` = float array; `*(pose+0xc)` = matrix array) — in `FUN_0020ad00`-like code the pose float base is `*unaff_s2`. With that pointer in hand, dump 0x200 bytes at it and the rider index. Two savestates one frame apart taken at the break would also give the exact input/output pair (channels → matrices) to settle the Euler order for legs. If a write breakpoint never fires, hypothesis 1 holds and the palette must be captured from VU1 memory (`0x358 + 4·bone`, 15 matrices) with a savestate taken while the rider microprogram (`.vutext` 0x0c8d) is running, e.g. a VU1 `MSCAL` breakpoint.

## Approximation H4 (2026-10-02)
The native body pose is still unlocated, so `DH_RIDER_POSE=approx` is explicitly
an approximation, not a claim about the game's pose. `src/rider.hpp` now solves
the four effectors with forward kinematics plus damped Gauss-Newton: wrists 5/8
use shoulder+elbow channels (12..15 and 19..22), and ankles 11/14 use
hip+knee channels (26..29 and 33..36). Wrist/ankle channels are not included
in the Jacobian. Knee and elbow channels are bounded to `[0, pi]`; residuals
are reported instead of clamped away.

Bike and model frames are both documented as X-right/Y-forward/Z-up. The pelvis
in bike space is the explicit `DH_RIDER_AT` parameter. Optimization uses a
coarse pelvis grid covering at least +/-2 units on every axis, expands to +/-4
when the best point is on an edge, and then performs local coordinate
refinement. Every forward-kinematics evaluation is counted. Candidates
violating the measured arm (1.824 u) or leg (2.77 u) reach are rejected as
geometrically invalid rather than silently accepted. Torso channels 6..8 are
refined locally; these values remain hypotheses, not ELF-derived pose data.

Increasing a hinge angle is measured through `worldPositions()` for each side,
with an explicit anatomical direction: elbows seek +Y (forward), while knees
seek -Y (backward). For ALP2 the measured ranges are: elbow 15 `[0,pi]`
(forward sign +), elbow 22 `[-pi,0]` (forward sign -), knee 29 `[-pi,0]`
(backward sign -), and knee 36 `[-pi,0]` (backward sign -). Hip flexion is
measured separately: +0.8 on channels 26 and 33 moves their ankle effectors
forward by about `1.9923` and `1.9920 u`.

The chain-to-pedal assignment is based on the measured pose-zero X coordinate,
not on the old palette label: the negative-X pedal is assigned to the
negative-X ankle and the positive-X pedal to the positive-X ankle. The same
side-by-position rule is used for the wrists.

The quality target is `<0.05 u` for each effector at phase 0 and phases
`pi/4..7*pi/4`. The optional ALP2 diagnostic records this criterion per effector. Remaining
failures are reported as residuals after side and anatomical-direction
correction, not as generic reach failures. The coarse stage evaluates the
at-least-±2 grid and the refinement reports its own evaluations and convergence
(`change < 1e-4`). This is a measured approximation diagnostic, not validation
of the game's pose.

Pelvis rotation channels 0..2 are also searched within `[-1.0, 1.0]` rad and
reported per phase. The current ALP2 per-phase results include rotations such
as `(0.6,0,0.6)` at phase 0 and `(0.35,0,0)` at phase `pi/4`; these are
approximation parameters, not recovered game data. `approxCycle()` provides the
shared-cycle mode: one pelvis translation, one pelvis rotation, and one torso
configuration are scored against all eight crank phases, minimizing the maximum
efector error.

For each residual case the test performs a 5^4 isolated sample over the four
measured limb channels (625 samples). Examples from the current ALP2 run:
ankle +X at `pi/4` has isolated best `0.466244 u`, wrist -X at `pi/2` has
`0.208819 u`, and ankle +X at `pi` has `0.391449 u`; these are classified as
shared-pelvis conflicts because a pelvis/torso search for the single limb finds
an error below `0.05 u`. “Infeasible” is reserved for a failure of that pelvis/
torso search. The isolated search is still a coarse diagnostic.

### Shared-cycle result and foot offset

The principal result is an experimental approximation, enabled only by
`DH_RIDER_POSE=approx`; it is not the game's pose. It uses one pelvis, pelvis
rotation, and torso for all eight crank phases. With the ankle at the pedal
center (`footOffset=(0,0,0)`), the selected shared pelvis is
`(0,1,0)`, rotation `(0.5,0.5,1.0)`, and the search made `125008`
evaluations. The rotation Z component is still on the expanded `+1.0` edge;
this is recorded as a range-bound experimental optimum, not a physical pose
claim:

| phase | wrist +X | wrist -X | ankle +X | ankle -X |
|---:|---:|---:|---:|---:|
| 0 | 0.727810 | 0.382811 | 0.353089 | 0.000000 |
| pi/4 | 0.727810 | 0.382811 | 0.513187 | 0.149539 |
| pi/2 | 0.727810 | 0.382811 | 0.000000 | 0.469974 |
| 3pi/4 | 0.727810 | 0.382811 | 0.348024 | 0.000000 |
| pi | 0.727810 | 0.382811 | 0.000000 | 0.000020 |
| 5pi/4 | 0.727810 | 0.382811 | 0.000000 | 0.520402 |
| 3pi/2 | 0.727810 | 0.382811 | 0.000000 | 0.000000 |
| 7pi/4 | 0.727810 | 0.382811 | 0.000000 | 0.000000 |
| worst | 0.727810 | 0.382811 | 0.513187 | 0.520402 |

The measured pose-zero foot geometry gives a ball-to-ankle offset of
`(0,-0.62,+0.37) u`: ball at `y=+0.51`, ankle at `y=-0.11`, and ankle height
`+0.37`. With this offset applied to the ankle target, the shared result uses
pelvis `(0,0,0)`, rotation `(0,0.5,0.5)`, and `125008` evaluations:

| phase | wrist +X | wrist -X | ankle +X | ankle -X |
|---:|---:|---:|---:|---:|
| 0 | 0.160114 | 0.656302 | 0.000000 | 0.179826 |
| pi/4 | 0.160114 | 0.656302 | 0.000000 | 0.000000 |
| pi/2 | 0.160114 | 0.656302 | 0.000001 | 0.735068 |
| 3pi/4 | 0.160114 | 0.656302 | 0.042353 | 0.337294 |
| pi | 0.160114 | 0.656302 | 0.189364 | 0.028711 |
| 5pi/4 | 0.160114 | 0.656302 | 0.337027 | 0.648340 |
| 3pi/2 | 0.160114 | 0.656302 | 0.000000 | 0.456194 |
| 7pi/4 | 0.160114 | 0.656302 | 0.276158 | 0.000000 |
| worst | 0.160114 | 0.656302 | 0.337027 | 0.735068 |

The offset changes the shared-cycle worst errors to `0.160114`, `0.656302`,
`0.337027`, and `0.735068 u`. These remaining failures are measured shared
pelvis/torso conflicts; the single-limb pelvis search finds feasible solutions.
This remains an experimental approximation, opt-in via `DH_RIDER_POSE=approx`,
not the game's pose.

The previously observed unconstrained diagnostic point `(1,0,0)` is rejected:
the left-hand anchor is `2.382679 u` from the pelvis, exceeding the 1.824-u arm
reach. At the accepted phase-zero pelvis, the anchor distances are `1.274633`,
`1.683853`, `1.984154`, and `2.126610 u` for right hand, left hand, right
pedal, and left pedal respectively; all fit the documented reach limits.

Cadence defaults to `5.5 rad/s` (hypothesis) or can be set with
`DH_RIDER_CADENCE`; in play mode the current implementation uses
`speed * 0.12` as an explicitly hypothetical transmission ratio.

## Viewer integration notes (2026-10-02) / notas del visor
`DH_RIDER_POSE=approx` is opt-in and experimental (hypothesis, not the game's pose). The pelvis and torso are optimised **once** at phase 0 (starting from `DH_RIDER_AT`, converted from the assembled-bike frame Y-up to the model frame Z-up) and then fixed; each frame only the four limbs are solved (about 0.01 s/frame instead of 0.1 s with a per-frame search, which also made the pelvis jump). `DH_RIDER_DEBUG=1` prints the per-limb error every 30 frames. The default (non-approx) placement keeps the model→assembled-bike mapping `(x, y, z) → (x, z, −y)`; an earlier version of the integration replaced it with the identity and laid the rider flat.
