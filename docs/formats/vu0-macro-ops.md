# VU0 macro-mode (COP2) and MMI operations in the rigid-body integrator / operaciones COP2 y MMI del integrador

Ghidra (language `r5900:LE:32:default`) disassembles these correctly but its decompiler loses them (`_sqc2`, `in_vf0`, lost arguments). Everything below is read from the **disassembly** (`tools/ghidra/run.sh ListInstr.java out.txt <addr>…`), citing addresses of `SLES_522.02`. Fixed register `vf0 = (0, 0, 0, 1)`; `ACC` is the VU accumulator; `.xyz` writes only x,y,z (w of the destination keeps its old value). `qmtc2 rt, vfN` loads the 128-bit GPR into `vfN`; `mfc1 v0, f20` + `qmtc2 v0, vf13` is the idiom "broadcast the float f20 through vf13.x". Rounding: the VU is not IEEE (flush denormals, truncation); the C++ port (`src/integrator_fidel.hpp`) keeps the order of the ACC chain but cannot promise bit-exactness (**hypothesis**).

## Catalogue / catálogo
| Op | Meaning | Used at |
|---|---|---|
| `lqc2 vfN, off(rs)` / `sqc2 vfN, off(rs)` | 128-bit load/store of a vector register | everywhere (`FUN_00238818` 0x2388c4…, `FUN_00237C78` stores `vf0` = (0,0,0,1) at +0xE0/+0xF0) |
| `qmtc2 rt, vfN` / `qmfc2.I rt, vfN` | GPR ↔ VU register (whole quad) | 0x2388d0 (dt → vf13.x), 0x227C8C / 0x227C94 (read `vf0`/`vf11` as GPR) |
| `vaddax.xyz ACC, vfs, vf0` | `ACC.xyz = vfs.xyz + vf0.x` = `vfs.xyz` (vf0.x = 0): load the accumulator | 0x2388d4, 0x238920, 0x238944 |
| `vmaddx.xyz vfd, vfs, vft` | `vfd.xyz = ACC.xyz + vfs.xyz * vft.x` | 0x2388d8 (`pos += vel*dt`), 0x238924 (`P += F*dt`), 0x238948 (`L += T*dt`) |
| `vmulax.xyz[w] ACC, vfs, vft` / `vmadday.xyz[w] ACC, …` / `vmaddz.xyz vfd, …` | `ACC = vfs*vft.x; ACC += vfs'*vft.y; vfd = ACC + vfs''*vft.z`: linear combination of three vectors with the components of a fourth | 0x238970-78 (`com·R`), 0x227874-0x22789c (`FUN_00227820`), 0x2277e4-0x227804 (`FUN_002277C8`) |
| `vmulx.xyz vfd, vfs, vft` | `vfd.xyz = vfs.xyz * vft.x` (scalar broadcast) | 0x2389f0 (`vel = P*invMass`), 0x22799c-0x2279a4 (`FUN_00227988`), 0x237d14 (`FUN_00237CF0`) |
| `vadd.xyz` / `vsub.xyz` | component-wise add/subtract | 0x237cfc (`P += J`), 0x237abc/0x237ad0 (`FUN_00237AB0`), 0x238990 (`nodePos = cm - com·R`) |
| `vmul.xyz` + `vadday.x/vaddaz.y/vaddax.z` + `vmaddz.x/vmaddx.y/vmaddy.z` with `vf15 = vaddw.xyz vf0,vf0 = (1,1,1)` | row·v dot products with the accumulator (x: (a+b)+c, y: (b+c)+a, z: (c+a)+b) | `FUN_002275F0` 0x227600-0x227624 → `out = A·v` |
| `vopmula.xyz ACC, A, B` + `vopmsub.xyz D, B, A` | cross product `D = A × B` (x = Ay·Bz − By·Az …) | `FUN_00228568` 0x228578, 0x2285cc, 0x228614 |
| `vmr32.xyzw vfd, vfs` | rotate words: `(x,y,z,w) → (y,z,w,x)` | 0x227C90 (builds (0,0,1,0) from vf0) |
| `prot3w rd, rt` | rotate the xyz words: `(x,y,z,w) → (y,z,x,w)` | 0x227C98, 0x227C9C (rows (0,1,0,0), (1,0,0,0) of the identity) |
| `pextlw rd, rs, rt` | interleave low words: `rd = (rt0, rs0, rt1, rs1)` | 0x227838, 0x227844, 0x227868 |
| `pextuw rd, rs, rt` | interleave high words: `rd = (rt2, rs2, rt3, rs3)` | 0x227854, 0x227860 |
| `pcpyld rd, rs, rt` | `rd = (rt.lo64, rs.lo64)` | 0x227840 |
| `pcpyud rd, rs, rt` | `rd = (rs.hi64, rt.hi64)` | 0x227850 |
| COP1 `mula.S / madda.S / madd.S` | `x²` , `+y²`, `+z²` in the FPU accumulator | `FUN_00228568` 0x228590-0x228598 (norm²) |
| COP1 `rsqrt.S fd, fs, ft` | `fd = fs / sqrt(ft)` (here 1.0/sqrt(norm²)); the EE reciprocal-sqrt is approximate (**hypothesis** on the exact rounding) | 0x2285a8, 0x2285f0, 0x228638 |
| COP1 `c.le.S` + `bc1t` | `if (dt <= 0) return` | 0x23882c, 0x23883c |

## How the PMMI shuffle in `FUN_00227820` works (transpose of a 3×3)
Inputs: `R0=(r00,r01,r02,r03)`, `R1`, `R2` rows of `A`. `pextlw/pcpyld/pcpyud/pextuw/pextlw` build `vf14 = (r00,r10,r20,·)`, `vf15 = (r01,r11,r21,·)`, `vf16 = (r02,r12,r22,·)`, the **columns** of `A`. Each input row `v` then becomes `v.x·col0 + v.y·col1 + v.z·col2 = A·v` (column vector); the 4th row passes through. Decoded by hand (PS2 MMI manual semantics above); the physically consistent result (R[i] += dt·ω×R[i], rows = body axes) is the only evidence — **no live consecutive states yet** (docs/formats/integrator.md, "What is validated").
