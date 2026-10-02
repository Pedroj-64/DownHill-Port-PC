# Rider leg channels (26-39): who writes them / quién escribe las piernas (investigation, unfinished)
Evidence: savestates 01 and 05 (EE RAM, outside the repo). Everything not cited = hypothesis.

## Files (tools/nga.py)
| file | clips | tracks on channels 26..39 |
|---|---|---|
| `BIKE/BANIM.NGA` | 198 (193 with 27 tracks, ch 0-26) | only ch 26 (194 clips). Odd clips: id 1 (ch 9-23), 2 (24-26), 3 (10, 17), 14 (none), 22 (ch 1) |
| `R/RANIM.NGA` | 206 (ch up to 114/116 = body 0-39 + fingers) | ch 26-39 in ~200 clips (33/34 only 21) |
| `SHELL/SHBANIM.NGA` | 4 | ch 26 |
| `SHELL/SHRANIM.NGA` | 4 | ch 26-32, 35-39 |
So RANIM *does* carry leg curves, but the live pose is not a RANIM clip (below).

## Live pose array (RAM)
`0x7ed430` (st05) / `0x7cf930` (st01): **117+ floats** (channels 0-116; 40-116 are the fingers) with mask at `+0x1e0` (`0x7ed610`); it is the output `pose` pointer (`+8`) of ~250 type-0x21 clip players (RANIM pool at `0x15d5cbc…`, BANIM players at `0x1660428/0x1660824`) and of the blenders `0x4f81c0`, `0x4f8bb8` (type 0x22). Only one such array exists in the state (shared; it holds the pose of whichever rider/rig was last evaluated, not necessarily the player).
* Channels **11-23** equal **BANIM clip id 129** (dur 24, t = 0) to 1e-3 in both states; clip 129 has no track on ch 0, 2-10, 17(0.10 live vs 0), 24-25 live differ.
* Not from any clip (live, differ between states): ch 1 = −0.3 (a literal, found in the ELF at `0x287088`/`0x2c2000`), ch 2-8 (pelvis/spine, vary), ch 17, 24, 25 (wrists), **ch 26-39 (legs)**.
* Legs live: st05 `26:-0.704 27:0.001 28:0 29:-0.020 30:0.014 31:-0.005 32:0.596 33:0 34:0 35:-0.561 36:1.349 37:-0.022 38:-0.018 39:0.353`; st01 `…26:-0.704 27:0.024 29:-0.167 32:-0.056 35:0.743 36:1.348 39:-0.361`. ch 26 is constant, ch 36 nearly constant (1.3485 / 1.3482: computed, not a literal), the others change with the state → **computed every frame (hypothesis: IK of the feet to the pedals)**. The knee hinge angles give hip-ankle distances 2.15 ft (L) and 2.76 ft (R, straight), consistent with one bent and one extended leg (coasting, cranks level).
* Forward kinematics of these values with the current Euler assumption does **not** give mirror-symmetric ankle positions (ankle L x = 1.7 ft off the hips): either the Euler argument order / axis of the left chain differs from the hypothesis in `rider.md`, or the legs are posed through another path (direct matrices). **Unresolved.**

## ELF search (no solver found yet)
Searched: functions storing floats at pose offsets `+0x68..+0x9c` (candidates were parameter-table initialisers: `FUN_001573d0`, `FUN_00184f90`, `FUN_001b5ce8`, `FUN_00243410`, none relevant), `lui 0x4049` (π) users (`FUN_0024b058`, `FUN_0024a888`, `FUN_00171770`, `FUN_00146368`, `FUN_0020df60`, `FUN_0020a6f8`, `FUN_001f0410`, `FUN_001b12f0`, `FUN_0014b950`, `FUN_0014ad18`: none is a leg solver), direct references to the literals. Next: find the caller of the skeleton walker that supplies the pose struct (`FUN_0020effc` case `0x11` @`0x20f4e8` reads `pose[ch]`), or step the physics frame in PCSX2 with a write watch on `0x7ed430+4·35`.
