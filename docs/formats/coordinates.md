# Coordinate conventions / Convención de coordenadas
Resolves the "start slots spread along Y" mismatch. Evidence is numeric (`tests/test_alp2_orientation.py`, skipped without game data) plus engine code; uncited = hypothesis.

| Space | Up axis | Notes |
|-------|---------|-------|
| NGP node space, world (nodes, `.PTS`, `.PTS` siblings, collision mesh `0x2A`, gate planes, start grid) | **+Z** | One shared space, **also the EE RAM space** (savestate positions match the collision mesh directly). **1 u = 1 ft = 0.3048 m** (confirmed: docs/p2s-savestates.md). |
| `.mdl` written by `tools/extract_model.py` | **+Z** (raw NGP coordinates, nothing converted) | Until now `dhview` treated it as Y-up; the model looked plausible but was lying on its side. |
| `dhview` internal space | +Y | Since this change `dhview` converts at load: `(x, y, z) → (x, z, −y)` (rotation, det +1, winding preserved) for model vertices, selector centres, sky, `DH_PTS` overlay, `DH_COL` triangles. The bike model (own space) is not converted. |
| `Ground` (`src/ground.hpp`) | +Y after `load()` | `load()` applies the same swizzle (`zup=true`); `set()` takes Y-up triangles directly. |

## Evidence
1. **Start grid** (`FUN_001A3480`, kind 8066 @ file 0x186AB0): 10 slots, local x spread 6 u/slot; the node matrix maps local x → world −Y, so the line of riders runs along **Y** (spread Y 54 u, X/Z < 5 u). A start line spreads laterally ⇒ Y is lateral.
2. **Gate orientation**: `FUN_001A3480` builds the start orientation with `FUN_00227CB8(euler)` using `(−0.2727, 0, π/2)` (non-wide kinds) or `(−0.284, 0, 0)` (wide) — a yaw of π/2 on the **third** component and a small pitch on the first, plus an eye offset `(0, −30, +30)` (`uStack_b0/ac/a8`), i.e. +30 u on Z = height. **Hypothesis**: Euler order (pitch, roll, yaw) ⇒ Z up.
3. **Collision normals at the start** (`tools/collision.py`, 501 triangles within 15 m of the grid): mean normal = (0.08, 0.02, **0.57**) with `(v2−v1)×(v0−v1)` (the winding that `FUN_002193C0` uses, see collision.md).
4. **Racing line**: from record 0 along `a6`, Δ = (+4087, +535, **−7825**) u: the course loses 783 m in Z and only 54 m in Y.
5. My earlier ground-height validation projected along Y; because the `.PTS` points lie on the surface, **any** projection axis returns the point itself, so that test was blind to the axis (the “11 u error” stands, but it never proved Y-up).

## Open
Handedness/sign of the `.mdl` U/V and the bike model's own up axis are untouched. Triangle winding sign `(v2−v1)×(v0−v1)` is chosen empirically (see collision.md): with it the start platform faces +Z (item 3) and the vertical-sweep validation passes.
