# V282 — Stable HemEnv dark-metal highlight: four separate gamma sites (source-only)

**Scope:** original licensed DSR RX33 Csd, RX34 Sdw, RX35 plain Stable HemEnv; PTDE is the visible target, DSR the host. This is **not** a shader activation, pixel fix, or new addon release. Continue on the single-addon PR #364 lineage.

## Independent source authentication

The owner's original `DSRRL_FINAL_EVIDENCE_PART02(2).zip` contains `DSR/shader/FRPG_FlverPBL_fpo_DX11.shaderbnd.dcx` (SHA-256 `ad180732ac79d5d98783aa504c789c2bab15e515c3b0f66b237d8b8c69113394`). Extract DCX DFLT at byte 0x4c, scan BND3 DXBC structures, verify exact full shader SHA, then parse SHEX tokens. All 3 full hashes match `pmetal_rgba_authority.hpp`. No original game shader bytes committed.

| Stable RX | Original DXBC SHA256 | Workflow=0 TRUE gamma 2.2 immediate | Workflow nonzero ELSE 2.2 immediate | SpecTex×cb10 ELSE 2.2 immediate | Post-LightScattering 2.2 immediate |
|---|---|---:|---:|---:|---:|
| 33 | `35880c0b2f2330208dfc21af6dd3d944218fcc4540cd8e59404a0aefc13c0b24` | 1216 | 1299 | 1327 | 2694 |
| 34 | `d6038de494509e7cbcbfb904c4046e9427f3b921f6a35735a0b0d316f9976837` | 1125 | 1208 | 1236 | 2613 |
| 35 | `7d03c75b69f5730eb741a4d327189d0bbed8a8450fb0ac04e1505f7b91763701` | 785 | 868 | 896 | 2266 |

Four exact stock `LOG -> MUL(2.2) -> EXP` sequences per RX; `IF_Z cb0[85].x` branch tracking verifies first site in TRUE arm, second and third in ELSE, fourth outside MaterialWorkflow.

## Source cut mismatch

* `renderer-core/data/provenance/material_response_diffuse_v1.tsv` has `diffuse_pow_site = 1299/1208/868` — exactly the **second** (ELSE) gamma, not the first (TRUE) gamma. The generic Material Response producer replaces only the three 2.2 immediate constants at that site with 1.0.
* `renderer-core/src/operators/env_spec/pmetal_rgba_materializer.cpp` `apply_pmetal_pre_ab` (V2.28A / R8) authenticates the **third** chain and NOPs LOG and MUL, EXP becomes MOV. Its input is `r1.xyz` live from the immediately previous `SpecTex * cb0[10]`; the in-place register transformation is semantically valid. The third chain is in the ELSE branch and cannot alone explain dark response on an actually executed Workflow=0 path.
* Stable R15/R16/R17 rewrite other MaterialWorkflow gates, a workflow-material MAD and common composition. **No direct replacement of the first TRUE-branch 2.2 chain is established by those functions.** In original RX33 that first EXP (word1220, r8.xyz) is read immediately downstream (MUL1225, ADD1247, MUL1271), so it is not a dead orphan in stock. Its final reconstructed R17 preFog flow is still **OPEN**.
* The fourth 2.2 is the stock post-LightScattering transform, an independent downstream operator. Do **not** remove it simply because highlights look dark.

### Narrow test required before changing pixels

1. Reproduce all three actual *FINAL Stable R17* DXBC SHA `b8a9f32010dd279ffae5dd3c787dceb4b06c696c3fe4007bd87ba8f3303e25ca`, `abb86881378803d3389bb42d63c97750865a62e23b1ede205bfb63795855ec30`, `023ad52a81399ff81066e55ff223ac9e4a90be7a64e21eb760ac5ba68f38c23b`. Assert the first chain persists, trace each RGB component through R15/R16/R17/R3/R11 to **pre-stock-Fog** live-out. Verify it is not zeroed/masked, and identify whether it feeds diffuse/common or the visible specular.
2. At an exact FLVER SHA+MTD raw SHA+material slot+bound PS+stock t1/PTDE t10 pair, capture **live** `cb0[85].x`. MTD authored zero is not proof of runtime zero. Verify PTDE sidecar/probe and branch routing.
3. Only if material-local causality matches PTDE, compare an *isolated* true-branch operator candidate against PTDE at matched pixels. Preserve `LOG abs`/SAT/negative-domain semantics where relevant and stock DSR Fog/LS/Glow. **No global exponent=1, no exposure/LightBank compensation, no blanket Stable RX patch, no V275 or V278 COLOR0 candidate.**
4. Validate beta0/beta1 Stable↔Lerp consistency; this V282 finding is not automatically transferable to Lerp R19. One reproducible addon descendant, complete fail-open gates and D3D11 state restore.

Status: **CONFIRMED stock shader identities + branch/site targeting. HIGH-CONFIDENCE source gap. OPEN final R17 reachability, runtime path, and PTDE-visible cause.** Supabase evidence `486a50ea-4570-4a3c-a724-c7c9172384fa`; canonical finding `renderer.stable_hemenv.v282_4x_pow22_census_materialworkflow_zero_branch_gap_v1` at revision 12994.
