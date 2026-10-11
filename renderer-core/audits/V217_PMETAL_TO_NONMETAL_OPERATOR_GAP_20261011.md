# V217 operator audit — P_Metal versus ordinary equipment
Date: 2026-10-11
Grounded source: V216 `7cb9fff1c9ceddd70f911b061299da7706ac073d`
Status: **CONFIRMED source-level gaps**, operator reuse **OPEN**, runtime/pixel equivalence **OPEN** for nonmetals.

## Scope and invariant

P_Metal is the existing dedicated PTDE operator island. Do not rewrite R15–R19 or treat a successful source/build/receiver match as PTDE pixel equivalence. Never promote equipment to P_Metal solely because it uses the same RX33–35 pixel shader; Material/MTD/FLVER/slot/source/probe must independently authorize the operator. Generic MR must continue to forbid feeding PTDE SpecRGB to unmodified DSR PBL/F0.

## Verified operator matrix

| Cut | P_Metal | Generic equipment | Finding |
| --- | --- | --- | --- |
| Exact material routing | `P_Metal[DSB].mtd`, raw MTD SHA `ece70f36bd2517d28c8495e276cea537f8b519d6bed981788e79a409ffbf763b`, route 345, exact owner/slot, RX33–35 | Exact owner/slot and MTD checks; `sidecar_20261010::exact_nonmetal_material` explicitly excludes P_Metal | Separate authority required |
| CPU producer | Exact material-aware `pmetal_env_source_runtime::latest()`, selector epoch, material-keyed producer; retail single/blend endpoint packer | No equivalent approved nonmetal `latest` endpoint producer in generic MR | **Missing nonmetal producer** |
| EnvSpec & EnvDiffuse probes | Semantic EnvSpec slot; PTDE probe pair, PTDE EnvDiffuse resources with same ordinal and distinct HemEnv/HemEnvLerp branches | Generic MR selects no matching env resource request/island | **Missing nonmetal resource operator** |
| Constant buffer ABI | Dynamic per-context `b12`: raw `c101`, `c100`, source A/B and beta; R19 adds EnvDiffuse endpoints; 64/96 byte layouts | Generic MR owns diffuse `c100` only; no equivalent EnvSpec CB source in generic draw | **Missing nonmetal CB semantic carrier** |
| Shader/operator | Exact PS SHA family RX33–35; stable R15 additive/postmerge, R16 diffuse alpha/workflow split, R17 material factor; separate R19 HemEnvLerp; PTDE EnvSpec/EnvDiffuse and appropriate composition | Generic MR materializes diffuse linear response and preserves the independent stock DSR PBL tail; no dedicated nonmetal EnvSpec PS selection | **Missing nonmetal PTDE operator island** |
| SpecRGB | PTDE sidecar `t10` is required by an independently approved shader consumer, stock `t1` retained; shadow-then-live SRV source check | `prepare_draw_requests(..., spec_rgb_consumer_ready=false)`, then defensive drop of any spec request | **Confirmed intentional anti-hybrid gate, not a sidecar loader bug** |
| Draw composition | One atomic PS+b12+PTDE resource batch; on any missing part preserve stock DSR draw, no partial P_Metal MR fallback | Generic MR can legitimately submit separate MR/diffuse/normal with spec disabled | **Different operator completion criterion** |
| HemEnvLerp | Distinct pixel shader/materializer and R19 endpoints; stability source `beta`/sample family checked | Distinct lerp MR; spec island also intentionally absent | Do not infer stable-only success |

## Confirmed code-level defect in experimental nonmetal lineage: authorization/producer contradiction

1. `renderer-core/src/runtime/pmetal_envspec_draw_runtime.cpp`, `exact_pmetal_envspec_candidate()`: under `DSRRL_V203_EQUIPMENT_ENVSPEC36_DIAG` it admits `v203_equipment::exact_envspec36_draw(material, decision)` (non-P_Metal original owners).
2. The same runtime's `prepare()` unconditionally calls `source_.latest(material, source)` before any EnvSpec resource or final request.
3. `renderer-core/src/runtime/pmetal_env_source_runtime.cpp` `pmetal_env_source_runtime::latest()` returns false if `!exact_pmetal_material_selection(material)` BEFORE looking up either synchronized state or retail hook fallback. This selection requires route 345, P_Metal MTD name and raw SHA.
4. Conversely `renderer-core/include/dsrrl/runtime/v203_equipment_envspec36_gate.hpp` explicitly rejects route 345/P_Metal at its own exact material gate.

Therefore a true nonmetal admitted by `exact_envspec36_draw()` **cannot pass the subsequent source stage**. This is a provable dead-end when the experimental flag is enabled, independently of texture naming and the t10 carrier. The flag is OFF in the V216 product path. **Do not solve by deleting the P_Metal source check**: retail hook source has P_Metal-specific provenance/epoch and would risk cross-material contamination. Build a separately authenticated source carrier, or demonstrate a shared exact producer via certified semantic mapping.

## Additional confirmed structural coverage blockers

- Generic `integrated_addon.cpp` stable and Lerp branches independently call `g_material_resources.prepare_draw_requests(..., false, ...)` for SpecRGB, then drop any stray `prepared.resources.spec_rgb`. This is a deliberate renderer PROTECT and must remain until a paired PTDE shader consumer is established.
- `material_resource_draw_runtime.cpp` generic Diffuse uses `h0 && h1 && generated::diffuse_pair_allowed_v12(h1,h0)`, Normal uses `h0 && h1 && h2 && generated::normal_tuple_allowed_v12(h0,h1,h2)`; 1295 Specular entries alone do not authorize either route. Preserve independent stock DSR Diffuse where homologous, rather than requiring new PTDE Diffuse assets for the 740 spec-only names.
- The separately-owned fixed direct PointLight path currently requires verified `spec_rgb && diffuse && normal` together, including paired t0/t1/t2 assets in `prepare_fixed_pointlight_material_requests`. This is a **PointLight carrier coverage limitation**, NOT permission to change PointLight's consumer without its separate workstream and proof.
- `renderer-core/src/runtime/material_response_draw_transaction.cpp` has a paired SpecRGB replacement bank for **prevalidated Subsurface only**. The generic MR replacement registries deliberately reject a spec owner. Do not reuse the Subsurface bank as a universal EnvSpec consumer.
- V204 already extends the **active loader** to the full 1295 Specular namespace. Continued texture-whitelist-only releases cannot resolve the missing operator.
- V214–V216 log markers cover conditional fallback/alternate-SRV success, not the direct cache-hit denominator. User's approximately 30-second armor-switch runtime is a valid test; engineer per-owner/draw records instead of asking for longer sessions.

## Reusable code and safe port boundaries

**Reusable verified source transforms** (subject to exact shader signature/ABI):
- Stable material response: `apply_ptde_materialworkflow_liveout_pair_r15`, `remove_dsr_materialworkflow_legacy_diffuse_split_r16`, `select_ptde_diffuse_material_factor_r17` in `renderer-core/src/operators/env_spec/pmetal_rgba_materializer.cpp`.
- Lerp operator: `apply_exact_ptde_lerp_materialworkflow_r15_r16_r17` in `pmetal_rgba_lerp_materializer.cpp`; its postcondition accounts for t10 sample insertion offsets.
- Exact authority: `pmetal_rgba_authority.hpp` pins stock PS SHA256 per RX33/34/35. It establishes **shader bytecode identity only**, not material/texture/probe authorization.
- General-purpose infrastructure: per-view companion loader, full FLVER/slot census, native SRV snapshot, draw transaction/state restoration and shader registries. Reuse these; do not duplicate.

**Non-reusable without separate proof**:
- `pmetal_env_source_runtime::latest` source values for arbitrary nonmetal (route345-bound).
- `P_Metal` raw MTD SHA/route, forced `envspc_slot=2`, probe ordinals/source or material F0/alpha response.
- P_Metal replacement shader as universal replacement for nonmetals with different MTD, shader family or receiver.
- P_Metal-specific cancellation of R15/R16/R17 DSR terms for an unrelated receiver, without confirming the exact PTDE-vs-DSR operator mismatch.

## Implementation order for the next actual functional addon

1. **Receiver-first census**: choose one nonmetal exact owner+slot+MTD and RX33–35 (e.g. already observed RX34 route160), determine its actual stock shader SHA, PS t0/t1/t2, c100/c101/c86 inputs and native source. Compare these against the exact P_Metal shader family and per-MTD operator semantics. No global promotion.
2. **Source producer parity**: add a nonmetal-specific, epoch-safe PTDE EnvSpec/EnvDiffuse source carrier keyed to exact FLVER SHA + slot + raw MTD SHA and map/branch. Do not borrow the P_Metal selector fallback. PASS requires real nonmetal source at first consumer.
3. **Shader variant reuse**: on the certified receiver only, apply existing R15/R16/R17 or R19 transform with unchanged DXBC length/identity/postconditions where structures match. Include the exact PTDE SpecRGB*c101*COLOR0 and EnvSpec/EnvDiffuse composition, with any nonhomology corrected operator-locally. Verify RDEF/checksum/CB/SRV ABI and downstream topology; materializer completion alone is not a pixel verdict.
4. **Atomic draw-only transaction**: bind verified pixel shader + c100/c101/source CB + relevant t10/EnvSpec/EnvDiffuse resources at one draw, preserve stock t1/other independent channels, restore all relevant D3D11 state. Do not edit DarkSoulsRemastered.exe on disk.
5. **Tests**: native negative oracles for wrong FLVER/slot/raw MTD, wrong RX, wrong PS SHA, missing producer, wrong probe, missing t10, mismatched stable vs Lerp, destroyed SRV; short user equip-switch runtime plus downstream PTDE comparison. Keep CONSTRUCTION/COMPATIBILITY/LIVENESS/ACTIVATION/PIXEL distinct.

### Status snapshot

- P_Metal R15–R19 code, source producer, PTDE resource/CB and atomic draw construction: **CONFIRMED in source**; previously observed runtime liveness, not blanket PTDE pixel PASS.
- Nonmetal experimental 36-owner path source rejection: **CONFIRMED conditional code defect**.
- Generic nonmetal PTDE SpecRGB EnvSpec operator island: **OPEN / not implemented**.
- Additional nonmetal shader/producer equivalence and pixel outcome: **OPEN**.

This file is a source-audit result only. Do not promote it as a new production addon/release or as a new gameplay test.
