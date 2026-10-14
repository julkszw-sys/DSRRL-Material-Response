# V281 — PTDE R5 Normal Producer: exact Stable R17 → HemEnvLerp R19 transplantation

## Confirmed FIRST operator mismatch on actual final native shader output

This work follows the V264 handoff and the V280 stable-vs-Lerp continuity contract. It uses original owner DSR binder `FRPG_FlverPBL_fpo_DX11.shaderbnd.dcx` (original DCX SHA `ad180732ac79d5d98783aa504c789c2bab15e515c3b0f66b237d8b8c69113394`). The original stable RX33/34/35 DXBCs are identified by full SHA from `stable_hemenv_receiver_census_v1.json`; original Lerp RX33/34/35 and actual production-final R19 SHA are the V276 authority.

I **compiled and executed the actual Stable R17 materializer** using exactly the same 2026-10-13 V272 CMake configuration macros as the R19 source (R3/R5/R7/R11/R15-R17 enabled, Upper/Lower OFF). All three original stable PS inputs returned `applied`, with SHA:

| Receiver | Actual complete Stable R17 SHA256 | bytes | Final Lerp R19 SHA256 |
|---|---|---:|---|
| RX33 Csd | `b8a9f32010dd279ffae5dd3c787dceb4b06c696c3fe4007bd87ba8f3303e25ca` | 23328 | `1d9fed8a55689e09bacf55a66a88f2c3474f7c3f9a5604fe5151b0506b06caab` |
| RX34 Sdw | `abb86881378803d3389bb42d63c97750865a62e23b1ede205bfb63795855ec30` | 23028 | `8f86b1aec73b3b9ec77c10ce6de2308d0b0d4533a1430373ea92b297ee387f97` |
| RX35 plain | `023ad52a81399ff81066e55ff223ac9e4a90be7a64e21eb760ac5ba68f38c23b` | 19872 | `b5f94ccec4e01e7d98c4329c2b28aecfd65e103729be969a53f486b182c55b70` |

For reproducibility, Stable was built with Upper/Lower physically OFF and a **test-only throwing stub** for `augment_upper_lower_hemenv_verified_base`, never invoked because `compose_upper_lower=false`. This accurately reproduces the exact non-U/L production path but does **not** test independent Upper/Lower. That stub is NOT part of any addon or PR.

The **first concrete spatial-operator discontinuity** appears before the EnvDiffuse/EnvSpec consumers:

- Stable R17 R5 uses PTDE-style `normalize(cross(N0,T) * tangent.w)` to construct bitangent, and removes DSR `SV_IsFrontFace` normal sign negation.
- Final Lerp R19 still uses stock interpolated `vB` normalizing path (19 DWORDs), and the same frontface `MOVC` with NEG extended operand `0x41`.
- Stable and Lerp frontface shader inputs have **matching register numbers** RX33 normal v2/tangent v4, RX34 normal v3/tangent v5, RX35 normal v2/tangent v4. Matching register index alone is not proof the matched VS supplies identical physical world values.

## Implemented, auditable *normal-operator candidate* (one narrow carrier)

`renderer-core/tools/v281/reconstruct_lerp_r5_from_stable_final.cpp` authenticates BOTH original Stable R17 and final Lerp R19 full 64-digit shader SHA per RX. It copies the **exact 43-DWORD** Stable production R5 operator from each authenticated stable PS into the corresponding actual final Lerp R19 (replacing the known 19-DWORD stock bitangent normalization), changes only the frontface NEG flag from 0x41 to 1, updates SHEX length / container size / checksum and shifts chunk offsets. All other SHEX words and every non-SHEX chunk remain byte-identical, including RDEF, input/output signatures, stock Fog/LightScattering and Glow output1 RGB/alpha. No VS modification, no EXE modification.

| RX | Final R19 patch size | V281 exact output SHA256 |
|---|---:|---|
| RX33 | 22608 | `552ae40c58300ec63a398a4d92e5e5c926694623756a95d2c5df7e7e36e1af1a` |
| RX34 | 22308 | `2bdfeac13fcce953bfa39b8fb299e79dfdb99cd2bb72d6a20a9d66ce052efc22` |
| RX35 | 20680 | `fd364e4eaef350f2cbd86bb6fc4aa4b56a5e40f08e8e646b7951897174e772ca` |

Owner-local 3/3 positive tests used the source downloaded back from successful [GitHub CI run 38505600620](https://github.com/julkszw-sys/DSRRL-Material-Response/actions/runs/38505600620) rather than a hand-edited tool. Resulting full SHA matched a separate initial local implementation on all RX. Independent Python verifier on owner-local files checked 43-word Stable↔Lerp operator equality, original other SHEX equality, original exact 212-word stock Fog/LS and both Glow target1 writes preserved. CI independently compiles the committed tool and rejects synthetic, unauthenticated shader inputs. Proprietary original/final/candidate DXBC never committed or included in CI artifact.

## Status and bridge requirements

- **CONFIRMED** first source-level spatial normal producer mismatch Stable R17↔ final Lerp R19 on three exact receiver shader identities.
- **CONFIRMED** exact R5 normalized-bitangent + no-frontface-neg transfer to three authenticated rebuilt shader *candidates* with unmodified other branches.
- **OPEN** actual D3D11 WARP `CreatePixelShader` on the owner-derived outputs; shader format/driver safety need native acceptance before runtime.
- **OPEN** exact physical vertex semantic equivalence and draw-specific FLVER SHA + MTD SHA + material slot + original PTDE texture companion, plus exact stock t1/t2 owner and receiver family.
- **OPEN** Stable R7 shadow visibility Csd/Sdw, R3/R11 postmerge equivalence, source endpoint beta0/1, and **PTDE-visible pixels**.
- **NOT ACTIVATED:** do not apply patched shader globally by RX family. Actual runtime bridging must gate exact equipment owner and preserve full per-draw D3D11 state. Non-owned world/body/shared RX must remain stock DSR. This is source construction and true first-operator parity of R5 code, **not a shipping mod or pixel PASS**.

The previous V278 speculative extra `COLOR0` multiplier is not required from absence of an adjacent MUL: V279 proved an existing nonlocal COLOR0→material response path. It is **not** combined with this R5 candidate. Both Stable and Lerp remain paired scope; R43 active host remains unchanged.
