# DSRRL Renderer Edition — PointLight R36 Full Handoff

Date: 2026-10-05
Scope: clustered PointLight / PTDE source carrier / Spc hybrid / R36 performance fix
Repository: `julkszw-sys/DSRRL-Material-Response`
Active development branch: `r19-lerp-exact-envdiffuse-ab-beta`

## Purpose

This is the transfer point for the next agent. Current work restores PTDE PointLight source behavior on DSR while keeping the bridge receiver-scoped, fail-open where identity is not proven, and performant enough to ship. The next expected input is an R36 ReShade runtime log plus possible owner observation about FPS/stutter.

The next agent must keep separate:
- CONSTRUCTION
- COMPATIBILITY
- RUNTIME LIVENESS
- BRIDGE ACTIVATION
- PERFORMANCE
- PIXEL / VISIBLE BEHAVIOR

Build PASS != runtime PASS != pixel PASS.

## Mandatory protocol

Before any renderer/addon work:
1. `dsrrl.agent_bootstrap(topic)`
2. `dsrrl.renderer_addon_control_plane_status(relevant_build_key)`
3. Read current canonical findings, PROTECT state, rejected/superseded history and current revision.
4. New knowledge: raw evidence -> finding -> promote only if justified.
5. Before final: `dsrrl.latest_revision()`, `dsrrl.knowledge_hygiene_audit()`, control plane again.

Do not modify DarkSoulsRemastered.exe on disk.

## Current R36 reference

Source commit:
`332e8bbfcac07d3448d0d1cdf6307aedfb9a0c07`

Supabase build:
- ID: `216`
- key: `pmetal_r36_pointlight_source_cache_332e8bbf`
- version: `R36-PL-SOURCE-CACHE-332e8bbf`
- parent: R35 build 215

Build status:
- Construction PASS
- Native build PASS
- Runtime NOT_TESTED
- Performance OPEN
- Pixel OPEN

CI:
- workflow: `.github/workflows/pmetal-r19-lerp-exact-envdiffuse-fast.yml`
- run: `37317685226`
- result: SUCCESS
- GitHub artifact: `11349830045`
- ZIP: `DSRRL_PMETAL_R36_POINTLIGHT_SOURCE_CACHE.zip`
- ZIP SHA256: `e0cb3d635645ca8b2b32490023a34093df04d6f61e5c63a137af3a186bd532ba`
- addon64 SHA256: `950264fae59ba7ae99ab7df35348eedfed126c6e49e3e8268aef063220aac4b4`

Supabase artifacts:
- ZIP 1392
- addon64 1393

Evidence:
`8038f026-6bc1-406a-80fa-eccf7b934aa2`

Canonical:
`renderer.pointlight.r36_source_cache_build_attested_v1`
finding `c2371d5e-337f-46bf-b868-3432b9ce3d58`
rev `11341`

## PointLight architecture

Shader replacement is create-time, not per draw.

`DSR creates exact known PointLight shader -> exact identity -> create-time replacement DXBC -> persistent pipeline object`

Dynamic PointLight data is routed around selector/draw time:

`FLVER/material identity`
-> `clustered_pnts_selector_event_bridge`
-> source stage `clustered_pnts_selector_source_event_bridge`
-> `selector_source_event()`
-> material stage `clustered_pnts_selector_identity_event_bridge(identity, pointlight_spc)`
-> `selector_identity_event(...)`
-> sidecar/GPU carrier
-> replacement shader
-> native/original draw where possible
-> state restore.

R34 separated source and material stages.
R35 allowed both NoSpc and Spc to consume the PTDE source stage.
R36 keeps those semantics and optimizes source production.

## Current Spc policy

Owner explicitly authorized:

`PTDE PointLight source/attenuation -> stock DSR Spc/GGX/Schlick material-response tail`

This is allowed but MUST be labeled:

`HYBRID_NOT_PTDE_LOCAL_SPEC_EQUIVALENT`

Old anti-hybrid PROTECT:
`protect.renderer.pointlight.clustered_spc_no_partial_legacy_specular_hybrid_v1`

Current:
`protected=false`

Disabled at rev:
`11325`

Do not re-enable the R33 pre-source Spc fail-open unless owner changes scope.

## PTDE PointLight source carrier

DirectPointLightEntity:
canonical `renderer.pointlight.direct_source_cpu_carrier_homology_v1`, rev 11294.

DSR retail source vfunc:
RVA `0x55C570`

PTDE homologue:
VA `0x00D34D50`

Exact carrier:
`{ position.xyz, 1/(End-Begin), RGB/source signal, End }`

Direct remains on its exact homologous native packer in R36.

Bank / LerpBank:
R36 preserves host position lanes and reconstructs PTDE donor carrier.
Attested DSR packers:
- Bank base+`0x55BC00`
- LerpBank base+`0x55D0B0`

Position:
- Bank node+`0x60`
- LerpBank node+`0x70`

Donor fills PTDE invRange/RGB/End.
Donor miss falls back to stock packer.
No arbitrary gain.

## R32 performance localization — do not redo

Canonical:
`renderer.pointlight.performance_source_capture_nested_selector_hotpath_v1`
rev 11306.

Authenticated R32 timing:
- producer avg 1491.379 us
- producer max 4568.100 us
- source capture 2307.744 us
- first-four selection 10.307 us
- sidecar build 0.157 us
- draw prepare 9.200 us
- GPU cache 0.300 us
- upload 3.500 us
- whole sync transaction 6.400 us
  - begin/capture+mutate ~5.000 us
  - restore ~1.200 us

Dominant regression = synchronous source capture nested in FLVER selector path, not draw transaction/upload/sidecar.

Do not repeat closed broad performance A/B tests.

## R33 -> R36 evolution

### R33
Spc failed open before source reconstruction.
Runtime confirmed Spc terminated at `cached_authority_reject`.
This avoided expensive Spc capture but Spc did not receive PTDE source.

Canonical:
`renderer.pointlight.r33_clustered_spc_runtime_failopen_verified_v1`
rev 11314.

### R34
Separated:
- source stage `selector_source_event()`
- material stage `selector_identity_event()`

NoSpc used PTDE source; Spc still failed open.

Canonical:
`renderer.pointlight.source_material_stage_split_v1`
rev 11321.

### R35
Owner authorized hybrid.

R35 routing:
`clustered_pnts_selector_source_event_bridge();`
then
`clustered_pnts_selector_identity_event_bridge(identity, pointlight_spc);`

Both NoSpc and Spc consumed PTDE source.

Authenticated R35 Spc runtime:
`decision_active_cached -> sidecar_ready -> direct_current_native_applied -> applied`

Owner reported severe performance degradation.

Canonical:
`renderer.pointlight.r35_spc_hybrid_runtime_perf_regression_v1`
rev 11336.

Because native current draw was already used, old deferred replay fallback is not the active bottleneck for that hit.

### R36
Keeps R35 hybrid semantics and changes source hotpath only.

## R36 performance changes

### 1. Remove Bank/Lerp duplicate pack

Before:
`stock DSR packer -> PTDE donor reconstruction`

R36:
- read only host position lanes,
- PTDE donor reconstruction,
- if donor succeeds, no stock packer,
- if donor fails, stock packer fallback.

Startup marker:
`[DSRRL POINTLIGHT R36] bank_lerp_native_double_pack=OFF frame_source_cache=TLS_EXACT_STATE_PER_PRESENT donor_miss_stock_fallback=ON`

### 2. 32-entry TLS source-carrier cache

Capacity:
32

Scope:
thread-local

Epoch:
per presented frame

Present callback:
`g_clustered_pnts.frame_event(present);`

Not a persistent global cache.

### 3. Exact state snapshot

Reuse requires exact match of:
- node pointer
- attested source vfunc
- owner pointer
- source ID
- source category
- source class
- selector word 0
- selector word 1
- position bits

One-shot cache hit marker:
`[DSRRL POINTLIGHT R36] frame_source_cache_hit=1 exact_state_snapshot=ON`

Presence proves at least one exact reuse.
Absence alone does NOT prove failure.

Possible absence causes:
- no repeated cacheable Bank/Lerp source,
- Direct-only sources,
- exact state changed,
- path not exercised,
- different TLS thread,
- session ended before repeat.

## Startup authentication for the next log

Expected markers:

`[DSRRL POINTLIGHT R34] source_material_split=ACTIVE source_stage=selector_source_event material_stage=selector_identity_event`

`[DSRRL POINTLIGHT R35] owner_authorized_spc_hybrid=ACTIVE clustered_nospc_consumes_ptde_source=ON clustered_spc_consumes_ptde_source=ON spc_material_tail=STOCK_DSR_GGX_SCHLICK local_specular_equivalence=OPEN`

`[DSRRL POINTLIGHT R36] bank_lerp_native_double_pack=OFF frame_source_cache=TLS_EXACT_STATE_PER_PRESENT donor_miss_stock_fallback=ON`

Source commit must authenticate as:
`332e8bbfcac07d3448d0d1cdf6307aedfb9a0c07`

If source/flavor differ, do not use the log as R36 evidence.

## Next-log interpretation order

### 1. Build authentication
Authenticate exact R36 binary and markers.

### 2. Runtime liveness
Loaded addon != active PointLight.

### 3. Receiver/material gate
Relevant stages:
- `cached_authority_reject`
- `decision_active_cached`

In R36, `cached_authority_reject` is NOT the old R33 intentional Spc anti-hybrid gate. If expected Spc now rejects, inspect exact owner/material/receiver/selector routing.

### 4. Source stage
Inspect:
- source capture success/fail telemetry
- source failure reason lines
- R36 cache hit marker

Failure family can still use older prefix:
`[DSRRL POINTLIGHT R29] source_capture_fail reason=...`

Do not infer material bug from source capture failure.

### 5. Cache reuse
Preferred:
`[DSRRL POINTLIGHT R36] frame_source_cache_hit=1 exact_state_snapshot=ON`

### 6. Sidecar / draw apply
Healthy chain:
`decision_active_cached -> sidecar_ready -> batch_ready/equivalent -> direct_current_native_applied -> applied`

If `direct_current_native_applied` is present, do not blame old replay fallback without new evidence.

### 7. Performance
Production R36 does not carry the old R32 profiler.

ReShade log timestamp gaps are NOT callback durations.

Use logs to prove routing/activation/cache behavior.
Use voluntary owner FPS/stutter observation as performance evidence.

Do not require new controlled runtime/captures as a solver prerequisite.

## Decision tree

### A. R36 applies + cache hit + performance restored
Promote:
- runtime liveness PASS
- bridge activation PASS for observed route
- cache mechanism runtime-confirmed
- performance PASS for observed scenario if owner/runtime evidence supports it

Pixel remains OPEN unless PTDE-visible behavior is actually validated.

### B. R36 applies + cache hit + performance still bad
Do not repeat broad bisects.

Next narrow suspects:
1. first/cold source capture once per unique light per frame remains too expensive,
2. donor authority/source-manager traversal,
3. many unique Bank/Lerp lights defeating cache,
4. cache key churn from legitimately changing state,
5. DirectPointLightEntity dominance (Direct is not principal R36 cache target),
6. unrelated renderer operator.

Allowed next work:
narrow sampled/thresholded timing around cold source capture + exact cache hit/miss accounting.

### C. R36 applies + no cache hit + performance bad
Find why reuse does not happen.

Check:
- Bank/Lerp vs Direct,
- recurring node/source IDs,
- selector/owner/position churn,
- TLS/thread distribution,
- one-consumption-per-frame pattern,
- capacity pressure.

Repeated Bank/Lerp with no hit => cache key/epoch bug.
Direct-only => R36 cache is not expected to help.

### D. R36 expected Spc -> cached_authority_reject
Routing/authority issue, not performance.
Old anti-hybrid gate is disabled.
Inspect exact owner/material/receiver/current selector identity.

### E. source_capture_fail
Classify exact reason.
Donor miss should stock-fallback for otherwise attested Bank/Lerp.
Unknown/unattested class/state should fail open.

### F. no PointLight activation
Do not call dead if session did not exercise an authorized PointLight path.
No new user runtime test may be a blocking prerequisite.

### G. performance good, visuals wrong
Do not repair pixels in performance code.

Separate:
- source semantics
- attenuation
- diffuse response
- Spc hybrid local-spec response
- EnvSpec

If range/color/diffuse are right but Spc highlight is wrong, remaining mismatch is likely the intentionally stock DSR GGX/Schlick tail.
That requires a real PTDE local-specular island, not arbitrary gains.

## Relevant PROTECT state

Still ACTIVE:
`renderer.runtime_v2.performance_bisect_closed_tests_do_not_repeat_v1`

Closed tests:
- ADDON_LOADED_ONLY
- DRAW_CALLBACK_ONLY
- POINTLIGHT_ONLY_BYPASS
- UL_H3_SUBSURFACE_PHYSICAL_CUT
- IDENTITY_VS_RESOURCE_SPLIT
- FLVER_VS_TEXTURE_SPLIT
- SELECTOR_VS_SUPPORT_SPLIT
- OLD_REPLAY_FAMILY_LOCALIZATION

Still ACTIVE:
`renderer.runtime_v2.no_duplicate_performance_bisects_without_lineage_change_v1`

Still ACTIVE:
`protect.project.no_new_capture_or_runtime_test_dependency`

A voluntarily supplied log can be consumed. Do not make a new test/capture prerequisite.

## Independent renderer state

Current P_Metal lineage:
- HemDir3 physically cut/OFF
- Subsurface physically cut/OFF
- Upper/Lower physically cut/OFF

Do not re-enable them while fixing PointLight.

Material Response and EnvSpec are independent and may remain active.

Do not repair:
- PointLight via Bloom
- PointLight via exposure
- range via arbitrary intensity
- Spc via global LightBank gain
- EnvSpec via PointLight changes

## SpecRGB reminder

Equipment-only SpecRGB scope remains protected.

Do not make clustered PointLight automatically inherit equipment SpecRGB sidecars just because Spc is active.

Owner-authorized Spc hybrid does NOT authorize PTDE SpecRGB into surviving DSR PointLight GGX/Schlick tail.

## What NOT to do

Do not:
- revert all PointLight to stock DSR because R35 was slow,
- re-enable R33 Spc fail-open without owner scope change,
- recreate shaders per draw,
- treat log timestamp gaps as execution duration,
- repeat closed broad perf A/B tests,
- blame replay fallback when `direct_current_native_applied` is present,
- globally widen receiver matching,
- disable unrelated DSR systems,
- use arbitrary light/specular gains,
- infer pixel equivalence from cache hit,
- infer runtime PASS from build PASS,
- infer pixel PASS from runtime PASS,
- require new screenshots/runtime tests to continue static work.

## If R36 performance still fails

Preferred direction is to move source preparation farther away from material draw frequency.

Desired architecture:

create-time:
`replacement shader`

source update/frame producer:
`resolve PTDE PointLight source carrier once per changed/visible source`

draw:
`select prepared carrier -> bind -> native draw`

But do not create a global persistent cache without proving:
- object lifetime
- source identity
- update timing
- map/load invalidation
- thread ownership
- source-manager mutation semantics

R36 is deliberately conservative:
TLS + exact-state + per-present epoch.

## Relevant source files

- `renderer-core/src/runtime/clustered_pnts_draw_runtime.cpp`
- `renderer-core/include/dsrrl/runtime/clustered_pnts_draw_runtime.hpp`
- `renderer-core/src/runtime/flver_engine_hooks.cpp`
- `renderer-core/integrated/integrated_addon.cpp`
- `renderer-core/include/dsrrl/runtime/pointlight_ptde_source.hpp`
- `renderer-core/include/dsrrl/runtime/pointlight_ptde_source_runtime.hpp`
- `renderer-core/src/operators/point_light/clustered_pnts_direct_materializer.cpp`
- `renderer-core/tools/audit_active_islands_hotpath.py`
- `renderer-core/tools/audit_clustered_pnts_activation_sources.py`
- `.github/workflows/pmetal-r19-lerp-exact-envdiffuse-fast.yml`

## Important canonical keys

- `renderer.pointlight.direct_source_cpu_carrier_homology_v1` — rev 11294
- `renderer.pointlight.performance_source_capture_nested_selector_hotpath_v1` — rev 11306
- `renderer.pointlight.r33_clustered_spc_runtime_failopen_verified_v1` — rev 11314
- `renderer.pointlight.source_material_stage_split_v1` — rev 11321
- Spc anti-hybrid PROTECT disabled — rev 11325
- `renderer.pointlight.r35_spc_hybrid_runtime_perf_regression_v1` — rev 11336
- `renderer.pointlight.r36_source_cache_build_attested_v1` — rev 11341

Re-query current revision before making decisions.

## Previous R35 runtime evidence

R35 runtime log artifact:
`1390`

SHA256:
`fbbbc9db92b24cbaecb21ebb3e2b23da9b6942b11d5ad2bbef7fef565309dd60`

Observed:
- exact R35 hybrid active,
- Spc active,
- decision_active_cached,
- sidecar ready,
- direct current native apply,
- final applied,
- severe owner-observed performance regression.

This is the R36 baseline.

## Expected promotion from a good R36 log

If authenticated R36 log proves:
- correct build,
- PointLight liveness,
- intended Spc/NoSpc activation,
- cache hit or otherwise valid source path,
- native draw apply,

then update R36 control-plane metadata.

Potential:
- construction PASS
- native build PASS
- runtime liveness PASS
- bridge activation PASS for observed route
- performance PASS only with actual supporting runtime/owner evidence
- pixel OPEN unless PTDE-visible behavior is validated

## Takeover summary

We are restoring PTDE PointLight source behavior inside DSR. Shader replacement is create-time; dynamic PointLight carrier data is routed through a source stage and separate material-response stage. The owner explicitly allows clustered Spc as a hybrid: PTDE PointLight source/attenuation feeding stock DSR GGX/Schlick local-specular response. R35 proved that Spc route activates through the native current draw path but caused severe performance loss. R32 had already localized the dominant CPU cost to synchronous PointLight source capture (~2.3 ms on executing samples), not selection/upload/draw transaction. R36 keeps R35 hybrid semantics but removes duplicate Bank/Lerp native packing on PTDE donor success and adds a 32-entry TLS exact-state per-present source-carrier cache. R36 build 216 / source 332e8bbf... is construction/native-build PASS; runtime/performance/pixels are still OPEN. The next log must authenticate R36, then check authority/apply chain and the one-shot `frame_source_cache_hit=1 exact_state_snapshot=ON` marker. If cache hits and performance remains poor, do not repeat broad bisects: instrument only surviving cold/first-use source capture or identify Direct-source dominance/high unique-light churn. If performance is fixed, promote runtime/performance separately but keep pixel equivalence OPEN until visible PTDE behavior is proven.
