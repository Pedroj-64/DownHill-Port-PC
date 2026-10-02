# Rider mount: bike anchors / unión del piloto a la bici
Tool: `tools/rider_mount.py` (anchors, BIKESKEL joints, rigid fit; `python3 tools/rider_mount.py` = self-test). Evidence: `BIKE/CARTMX.NGP`, `BIKE/BIKESKEL.NGP`, savestate slot 05 (ALP2 race), ELF `FUN_00200010`. Uncited = hypothesis. Bike space = `assemble_bike.py` space (X right, Y forward, Z up, 1 u ≈ 0.28-0.30 m).

## Anchors measured from data (frame model, type-40 "attach" nodes)
`CARTMX.NGP` (frame, type 1 kind 903 → type 25 kind 38 → selector → group with four type-4 matrices, each → one type-40 node). The matrices are rest poses in bike space:

| kind | name (hypothesis) | position (x, y, z) | rotation rows |
|-----:|-------------------|--------------------|---------------|
| 2087 | pedal R | (0.488, −0.280, −1.025) | (0, .585, −.811), (0, .811, .585), (1, 0, 0) |
| 2090 | pedal L | (−0.488, −0.282, −1.025) | mirrored |
| 2081 | grip R | (1.035, 0.791, 0.978) | (−.014, .866, −.5), (−.263, .48, .837), (.965, .143, .221) |
| 2084 | grip L | (−1.024, 0.790, 0.978) | mirrored |

Same four numbers come out of **BIKESKEL** independently: the bar joint (idx 7: loc (0, 1.256, 0.099), channels 24-26, rest Euler (0.436, 0, 0)) + type-3 translate (±1.001, 0.153, 0.765) + type-4 kind 41/42 (30° about X) + type-4 attach (0.034, −0.289, 0.096) gives **(1.035, 0.792, 0.978)** and **(−1.024, 0.791, 0.978)**: error ≤ 0.001 u, using the engine convention (Euler angles negated, `FUN_0020effc` case 0x11 @`0x20f4e8`) and row-vector `child × parent`. This validates the Euler convention for the X axis and the composition order (the other two axes are still untested).

## BIKESKEL is the bike rig (and BANIM drives it): correction
`BIKESKEL.NGP` (type 1 kind 900 → type 25 kind 900 → joint table `0x1900`) is a joint tree of 8 joints with exactly the 27 channels of `BANIM.NGA`: idx0 root 6 DOF (ch 0-5); idx1 fork/head tube (ch 6-8, rest 0.436 rad = 25° rake = BANIM ch 6); idx2 front wheel spin (ch 9, under kind 36); idx3/idx5 cranks (ch 10 / 17); **idx4 / idx6 = pedal handles, 6 DOF (ch 11-16 / 18-23: Euler then translation)**; idx7 handlebar (ch 24-26, rest 0.436). BANIM clip constants equal these rest values (ch 11-13 = (1.571, 0.946, 1.571) = eul0 of idx4; ch 18-20 = (−1.571, 0.948, 1.571); ch 24 = 0.436). **So BANIM's 27 channels are bike/pedal/bar channels, not the rider body's pelvis…arms assumed earlier (`rider.md`).** The rider joint tables reuse the numbers 0-39 (shoulder R = 12-14 …) but they cannot be the same array as the bike's (ch 11-16 would be pedal R and shoulder R at once): the rider body pose array is still **unlocated**; `0x7ed430` (120 floats, mask/limits at `0x7ed610`, referenced by 127 clip players) holds the **bike** pose.
Live evidence (slot 05): `pose[14..16] = (0.488, 0.231, −1.063)` = translation row of the bike skeleton matrix for idx4 in RAM (`0x15f3810 + 64·4`), `pose[21..23] = (−0.488, −1.005, −1.083)` = idx6. Crank radius = (0.231 − (−1.005))/2 = **0.618 u** (= 17 cm at 0.28 m/u: a real crank). Pedal centre y = −0.387 (rest anchors y = −0.28).

## Linking code (ELF)
`FUN_00200010` (0x200010; strings "Can't link frame/fork to bike master" 0x2c0118/0x2c0140, "front wheel to fork" 0x2c0168, "rear wheel to frame" 0x2c0190): for a bike object `p`: `p[4]` (frame) is chained into the joint table `p[0]` by `*(table+0xcc) = frame` (child pointer slot, old value saved in `p[7]`), `p[5]` (fork) into `p[1]`'s `+0xcc`, the wheel part `p[6]` into two type-4 nodes `p[2]`, `p[3]` at `+0x50` (front/rear wheel mounts). Kinds: frame mesh 903 (type 25 kind 38), fork 904 (kind 39), wheel 905, master 900.

## Bike-to-world frame
Rider body node (`rider + 0x792C`, pos `+0x10`, rows `+0x40/50/60`) is the physics body; its local contact points (docs/p2s-savestates.md: front (0, 2.5, −2.6), rear (0, −1.4, −2.8)) agree with the assembled bike's hubs (Y +2.43 / −1.60, Z −1.84; wheel radius 0.75 → bottoms at −2.59) to 0.07-0.2 u in Y. **Hypothesis:** node frame = bike model space (identity), so anchors above are in node-local coordinates (world = `p·R_node + pos`).

## What is NOT solved
* Pelvis/seat anchor: no locator exists in frame, fork or BIKESKEL (only kinds 2081/2084/2087/2090). The rider's pelvis must be derived from the rider's own skeleton fit (feet on pedals, hands on grips), which needs the **rider-body pose array** (not found).
* The requested quantitative test (wrists on grips, ankles on pedals with the live RAM pose, minimise the distance over translation + yaw) could not be run validly: with the 0x7ed430 pose the rider channels are the bike's, so the residual would be meaningless. `rider_mount.kabsch` (rigid / yaw-only fit, self-tested) is ready: feed it the rider's wrist/ankle positions (from `rider_pose.skin_matrices`, bone 5/8 wrists, 11/14 ankles) and the four anchors (live pedals via `live_pedals`) once the body pose is known.
* Next lead: the type-0x21 clip players in the rider struct (`+0x30/+0x34` pairs) all reference node `0x15f7b10` (BIKESKEL kind 900) → the bike; the body rig needs its own pose pointer — search the rider's walker call (`FUN_0020effc` case 0x11, pose struct `sp+0x58`: `+4` floats, `+0xc` matrices) for who builds that struct for kind 4090-4110 nodes.
