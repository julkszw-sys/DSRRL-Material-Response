# DSRRL — P_Metal black armor / global cyan full handoff
Date: 2026-09-21
Project: Dark Souls Remastered — Restored Lighting / Renderer Edition
Target: PTDE-visible renderer behavior. DSR is host renderer.

## 0. Executive state

The black-armor workstream is **not solved yet at pixel level**, but the fault surface has narrowed sharply.

### Current best candidate
- **build111**: `material_response_1_45_pmetal_fresh_specrgb_v5_runtime`
- Display name: `Material Response 1.45 P_Metal Fresh SpecRGB V5`
- Construction: **PASS**
- Runtime: **NOT_TESTED**
- Pixel: **OPEN**
- Parent: build107 V3
- IMPORTANT: V5 is based directly on V3; it **does not inherit V4B atmosphere-domain edits**.

### Current root cause finding
Canonical key:
`renderer.dsr.pmetal_v3_stale_specrgb_material_carrier_v1`
Revision: **8198**
Status: **CONFIRMED**

V3/build107 and its descendants correctly bind/sample PTDE SpecRGB at t10, but the originally sampled register is overwritten before the final P_Metal material multiply. The injected V3 final material multiply therefore consumes unrelated/stale DSR temporaries:
- DXBC 33 -> stale `r11`
- DXBC 34 -> stale `r10`
- DXBC 35 -> stale `r9`

This means V3 **does not actually implement** the intended:
`SpecRGB_PTDE * c101_PTDE * COLOR0`
at the final material cut, despite correct sidecar routing.

That stale-register defect is a direct shared mechanism for the global cyan/rainbow phenotype.

## 1. Runtime falsifiers already established

### V3 / build108
- Black starvation is removed, but **all equipment P_Metal goes cyan/aquamarine**, not only player P_Metal.
- Therefore PlayerIns-only selector/probe hypotheses are not the current root cause.
- Canonical runtime localization:
  `renderer.runtime.pmetal_v3_global_cyan_shared_composition_v1`
  rev8178.

### V4B / build110
Build key:
`material_response_1_45_pmetal_ptde_domain_bridge_v4b_runtime`

V4B tried to repair the PBL atmosphere-domain difference:
- recover linear FogRGB from DSR `cb12.rgb=q^2.2`,
- keep the P_Metal surface legacy-linear through Fog/LightScattering,
- remove DSR post-LS `pow(2.2)`,
- add PTDE terminal SAT.

Owner runtime screenshot **still shows very strong cyan/aquamarine P_Metal**.
Therefore V4B is pixel FAIL and atmosphere-domain mismatch is **not the dominant cyan cause**.

Build110 current status:
- construction PASS
- runtime PARTIAL
- pixel FAIL

Falsifier screenshot:
- Supabase artifact **1244**
- local path: `/mnt/data/image-1790001600586.jpg`
- SHA256: `7491eda1946d95303df77f6f7835fa2d2e01798ba6735e747002ffdfc7d26e20`

DO NOT continue tuning Fog/LightScattering to remove cyan unless new contradictory evidence appears.

## 2. V5 exact patch to continue from

Current build111 construction canonical key:
`renderer.build.pmetal_fresh_specrgb_v5_v1`
Revision: **8205**
Status: **CONFIRMED construction-only**

### Base addon
V3 exact EnvSpec basis:
`/mnt/data/v3_work/DSRRL_Material_Response_1.45_PMETAL_PTDE_SURFACE_ISLAND_V3_EXACT.addon64`
SHA256:
`def236254e8baf3727582f28d9fc24c03f16181e874b4bd922cbbe190782ed74`

### V5 output addon
`/mnt/data/DSRRL_Material_Response_1.45_PMETAL_FRESH_SPECRGB_V5_2026-09-21/DSRRL_Material_Response_1.45.addon64`
Size: **1,803,264 bytes**
SHA256:
`c1ec95e7942121a19173f6fceb391a5c4be9543bff3b58a1cad7e4e532833444`
Supabase artifact: **1245**

### Current V5 runtime ZIP
`/mnt/data/DSRRL_Material_Response_1.45_PMETAL_FRESH_SPECRGB_V5_RUNTIME_2026-09-21.zip`
Size: **264,253 bytes**
Current physical SHA256:
`1dce971d29c6289f846a4e0245ee03ec30a9b9bb28a4971edf7f60d93715426a`
Supabase artifact: **1247**
ZIP CRC: PASS

NOTE: artifact1246 is an earlier V5 ZIP snapshot from before the final deterministic rebuild. The addon bytes are identical, but package provenance is superseded. Build111 and canonical finding now point to artifact1247.

## 3. Exact DXBC patch coordinates

Only embedded DXBC indices **33, 34, 35** are modified.
All other 75 embedded DXBC containers remain byte-identical to V3.
All 78/78 DXBC checksums validate after patching.
Addon diff is confined to the three target containers.

### DXBC 33
- old original t10 SpecRGB sample word: **1316**
- fresh reacquire insertion word: **1652**
- final material MUL word: **1667**
- terminal RGB MOV word: **2822**
- stale V3 material operand: **r11**
- V5 material operand: **r2**, freshly reacquired t10 SpecRGB
- old SHA256: `222eac38b60b49ca9ff781c70453a3b11d9d567cc7d130f01170aa460094f950`
- V5 SHA256: `c898173cc7c63cf94453aaaf0478e5999021972c5c6e4270188316c13fa040e6`

### DXBC 34
- old original t10 SpecRGB sample word: **1225**
- fresh reacquire insertion word: **1561**
- final material MUL word: **1576**
- terminal RGB MOV word: **2741**
- stale V3 material operand: **r10**
- V5 material operand: **r2**, freshly reacquired t10 SpecRGB
- old SHA256: `c1ec33fa97b16c36ddfa07da6026bc8f86ffb99c599c7c14a1a49677060c5d87`
- V5 SHA256: `f77770b944e913c9c248495ab7738117f97f0d453839fa264bc37edb4e5c1df7`

### DXBC 35
- old original t10 SpecRGB sample word: **885**
- fresh reacquire insertion word: **1221**
- final material MUL word: **1236**
- terminal RGB MOV word: **2394**
- stale V3 material operand: **r9**
- V5 material operand: **r2**, freshly reacquired t10 SpecRGB
- old SHA256: `28ba1ea82d405eec1a07e55c043494dd440fd3f092d5c29418be4c3577fa3727`
- V5 SHA256: `61d28e36aa41ec61ac5070c899eb5ea31bb9a652d2037ea889e3e11598d5979e`

## 4. How V5 patch works

The V3 target bodies already contain a 15-dword NOP island immediately before the final material multiply.

For each of 33/34/35:
1. Read the original 11-dword `SAMPLE` that samples t10 with sampler s1.
2. Copy that exact SAMPLE into the NOP island immediately before the final material cut.
3. Change only destination to `r2.xyz`.
4. Leave the remaining 4 dwords of the 15-dword NOP island as NOPs.
5. Rewrite final material MUL source register from stale r11/r10/r9 to **r2**.
6. The next instruction continues to multiply by COLOR0 unchanged.
7. Set the SAT modifier bit (`0x2000`) on final separate RGB MOV.
8. Recompute the DXBC checksum.

The builder verifies:
- input DXBC checksum valid,
- original sample really is SAMPLE opcode,
- resource really is t10,
- sampler really is s1,
- NOP island is intact,
- final MUL destination is r2.xyz,
- old source register is one of r9/r10/r11,
- output checksum valid,
- output length unchanged,
- whole-addon diff remains inside three embedded target containers only.

## 5. Reproducible V5 builder

Primary script:
`/mnt/data/DSRRL_Material_Response_1.45_PMETAL_FRESH_SPECRGB_V5_2026-09-21/BUILD_V5_FRESH_SPECRGB.py`

Audit:
`/mnt/data/DSRRL_Material_Response_1.45_PMETAL_FRESH_SPECRGB_V5_2026-09-21/V5_RUNTIME_AUDIT.json`

Manifest:
`/mnt/data/DSRRL_Material_Response_1.45_PMETAL_FRESH_SPECRGB_V5_2026-09-21/MANIFEST_SHA256.txt`

DXBC checksum helper workspace:
`/mnt/data/blackfix_re/dxbc_checksum.py`

Standalone V3 extracted DXBC:
- `/mnt/data/blackfix_re/v3_33.dxbc`
- `/mnt/data/blackfix_re/v3_34.dxbc`
- `/mnt/data/blackfix_re/v3_35.dxbc`

Decoded/disassembly workspace is under:
`/mnt/data/blackfix_re/`

## 6. Sidecar requirement

Existing PackedGI sidecar remains required and is **not changed by V5**:
`DSRRL\EnvSpec\PackedGI\PTDE_GI_ENVSPEC_PACK_RGBA.bin`

Expected size: **33,619,968 bytes**
Expected SHA256:
`c16c3fd75bcf34f3cc075da6da1ad10c9440ee4a3ca580fe7f74d07a2ce4eac3`

The external pack / loader / GPU byte carrier has already been independently validated. Do not reopen BGRA/RGBA, pack SHA, loader or cube byte identity merely because a visible cyan symptom exists.

## 7. What is already closed / do not waste time on

- **Global cyan is not PlayerIns-only.**
- PackedGI pack conversion BGRA->RGBA is validated.
- Live P_Metal slot2 GPU bytes matched external pack byte-for-byte in prior audit.
- t12/t14 sampler descriptor was closed against PTDE.
- PTDE A/B c87 endpoint law and blend carrier were closed.
- PlayerIns packed-GI selector producer algorithm is homologous PTDE vs DSR; same-world endpoint value equality remains a separate OPEN issue, but cannot explain global all-equipment cyan alone.
- Pre-exponent environment visibility/shadow core is structurally homologous PTDE vs DSR; V3 removes only DSR extra exponent.
- Terminal RGB SAT is real PTDE behavior, but old SAT-only branches proved SAT alone does not remove the rainbow/cyan failure.
- V4B atmosphere-domain repair did not fix cyan; do not continue domain-tail tuning as the dominant root-cause path.
- No arbitrary EnvSpec gain, exposure, U/L, Fog or PointLight compensation.

## 8. Historical clue that strongly supports V5

The same class of bug occurred earlier in V2.21-V2.24:
- code treated an overwritten temp as if it still carried live SpecMap,
- resulting material/chroma isolator conclusions became invalid,
- direct def-use later proved the stale-register alias.

Canonical historical finding:
`renderer.dsr.pmetal_specmap_chroma_rainbow_carrier_v1` is REJECTED as literal SpecMap attribution because of the stale temp.

V3 accidentally reintroduced the same architectural failure class: correct resource exists, but the final material cut consumes the wrong register.

## 9. Why V5 should be tested before any broader rewrite

V3/108 and V4B/110 both exhibit a global cyan material phenotype.
V5 changes exactly one shared material-side semantic error common to all three P_Metal hosts:
`wrong stale temp -> fresh t10 SpecRGB`

It does not touch:
- U/L,
- PointLight,
- EnvDiffuse,
- Fog / LightScattering domain,
- cubemap sidecar content,
- c87 source,
- resource routing,
- s12/s14 sampler,
- other shader families.

This is the narrowest justified next runtime candidate.

## 10. If V5 still fails — next debugging order

1. Confirm V5 is actually active on all three hosts and that the patched DXBC hashes are the V5 hashes above.
2. Confirm the reacquired t10 SAMPLE uses the same original UV and sampler s1 as the original t10 sample.
3. Audit **COLOR0 / PTDE VertexSpec content homology** for the actual equipment FLVER route.
4. Audit the live c101 donor only if there is evidence it differs at the target draw.
5. Keep PTDE dynamic terminal `c135.x/c135.y` as OPEN. It can affect scale/clamp but should not be used as an arbitrary color fix.
6. Only after the fresh material lane is proven correct should broader whole-surface PTDE composition be reopened.

## 11. Explicitly OUT OF SCOPE

- Upper/Lower Ambient
- PointLight / player lantern
- tone mapping / exposure
- arbitrary EnvSpec gain
- broad Fog / LightScattering retune

## 12. Supabase state

Project id:
`bpcjpgwciohtidprncrd`

Before continuation:
1. `dsrrl.agent_bootstrap(topic)`
2. `dsrrl.renderer_addon_control_plane_status('material_response_1_45_pmetal_fresh_specrgb_v5_runtime')`

Key canonical findings:
- `renderer.dsr.pmetal_v3_stale_specrgb_material_carrier_v1` — rev8198 CONFIRMED
- `renderer.build.pmetal_fresh_specrgb_v5_v1` — rev8205 CONFIRMED construction-only
- `renderer.runtime.pmetal_v3_global_cyan_shared_composition_v1` — rev8178 CONFIRMED
- `renderer.cross.playerins_packed_gi_selector_producer_homology_v1` — algorithm homology only; endpoint values OPEN
- `renderer.cross.pmetal_v3_environment_visibility_preexp_homology_v1` — visibility core not supported as cyan root

Builds:
- build107 = V3 exact EnvSpec basis
- build108 = V3 runtime candidate; global cyan, pixel FAIL
- build110 = V4B; cyan persists, runtime PARTIAL / pixel FAIL
- build111 = V5; construction PASS / runtime NOT_TESTED / pixel OPEN

Artifacts:
- 1244 = V4B cyan screenshot
- 1245 = V5 addon
- 1247 = current V5 runtime ZIP

## 13. GitHub

Repository:
`julkszw-sys/DSRRL-Material-Response`

V4B archival branch:
`pmetal-v4b-domain-bridge-2026-09-21`

V5 handoff branch:
`pmetal-v5-fresh-specrgb-handoff-2026-09-21`

## 14. Methodology reminders

- PTDE visible behavior is target; DSR remains host.
- first-consumer equivalence != pixel equivalence.
- choose narrowest verified carrier.
- receiver-first / fail-open.
- do not repair material response through U/L, Fog, exposure or global gains.
- runtime PASS != pixel PASS.
- construction PASS != runtime liveness != pixel equivalence.
- do not promote V5 to release before runtime/pixel validation.

## 15. One-sentence continuation instruction

**Start from build111/V5, verify the fresh t10 SpecRGB reacquire at the final material cut on DXBC 33/34/35, test/validate that candidate first, and only if cyan persists move next to asset-level COLOR0/VertexSpec homology; do not inherit V4B atmosphere-domain edits or reopen PackedGI/resource routing.**
