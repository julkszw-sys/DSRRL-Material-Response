# DSRRL Renderer Edition — PointLight R36 Full Handoff
Date: 2026-10-05
Scope: clustered PointLight / PTDE source carrier / Spc hybrid / R36 performance fix
Repository: `julkszw-sys/DSRRL-Material-Response`
Active development branch: `r19-lerp-exact-envdiffuse-ab-beta`

## 0. Purpose of this handoff

This document is the transfer point for the next agent. The current task is not “find another PARAM that looks close”. The task is to preserve PTDE PointLight source behavior on DSR while keeping the bridge receiver-scoped, fail-open where identity is not proven, and performant enough to be usable.

The immediate next input is expected to be an R36 ReShade runtime log, possibly with an owner observation about FPS/stutter. The next agent must authenticate the build first, then determine whether R36:
1. loaded,
2. activated the intended clustered PointLight path,
3. actually reused its new source cache,
4. removed the R35 performance regression,
5. preserved the intended hybrid semantics,
6. changed visible PointLight behavior toward PTDE.

Do not collapse these into one “PASS”.

Status dimensions must remain separate:
- CONSTRUCTION
- COMPATIBILITY
- RUNTIME LIVENESS
- BRIDGE ACTIVATION
- PERFORMANCE
- PIXEL / VISIBLE BEHAVIOR

A build can pass construction and still fail performance or pixels.

---

## 1. Mandatory project protocol for the next agent

Before any further renderer/addon work:

1. Run:
   `dsrrl.agent_bootstrap(topic)`
2. Run:
   `dsrrl.renderer_addon_control_plane_status(relevant_build_key)`
3. Read current canonical findings, current PROTECT state, rejected/superseded history, and current revision.
4. Prefer current normalized addon control-plane state over old prose/history.
5. Any new knowledge:
   raw evidence -> finding -> canonical promotion only if justified.
6. Before final response:
   - verify `dsrrl.latest_revision()`,
   - run `dsrrl.knowledge_hygiene_audit()`,
   - rerun addon control plane.

Do not use build success as proof of runtime, performance or pixel equivalence.

Repository work must remain reproducible. Do not patch `DarkSoulsRemastered.exe` on disk.

---

## 2. Project-level target and operator rule

PTDE is the target visible rendering behavior.
DSR is the host renderer/runtime.
Vanilla DSR is a baseline/fail-open, not the artistic target.

The target is:

`PTDE state -> F_P -> PTDE visible result`

versus

`DSR host + bridge -> F_D^bridge -> DSR visible result`

and the bridge should minimize PTDE-visible residual using the narrowest verified carrier:
PARAM / SHADER / CB / RESOURCE / MATERIAL / ASSET.

First-consumer equality is not pixel equivalence.

For PointLight specifically, source and material response must be treated as separate semantic stages:
- PointLight source/carrier: position, range/attenuation inputs, source RGB/signal.
- material response: diffuse and local specular behavior of the receiver.

The current owner-authorized Spc implementation is intentionally a hybrid:
`PTDE PointLight source/attenuation -> stock DSR Spc/GGX/Schlick material-response tail`.

This is allowed by current project policy but must ALWAYS be labeled:
`HYBRID_NOT_PTDE_LOCAL_SPEC_EQUIVALENT`.

Do not describe the current Spc tail as a PTDE local-specular port.

---

## 3. Current reference build: R36

### Source
Runtime source commit:
`332e8bbfcac07d3448d0d1cdf6307aedfb9a0c07`

This is the exact source reference for R36 runtime behavior even if the branch later advances because of documentation commits.

### Build
Supabase addon build ID:
`216`

Build key:
`pmetal_r36_pointlight_source_cache_332e8bbf`

Display name:
`R36 PointLight Source Cache`

Version:
`R36-PL-SOURCE-CACHE-332e8bbf`

Parent:
R35 build `215`

### Build status
- Construction: PASS
- Native build: PASS
- Runtime: NOT_TESTED
- Performance: OPEN
- Pixel: OPEN

### CI
Workflow:
`.github/workflows/pmetal-r19-lerp-exact-envdiffuse-fast.yml`

GitHub Actions run:
`37317685226`

Result:
SUCCESS

GitHub artifact ID:
`11349830045`

Artifact:
`DSRRL_PMETAL_R36_POINTLIGHT_SOURCE_CACHE.zip`

ZIP SHA256:
`e0cb3d635645ca8b2b32490023a34093df04d6f61e5c63a137af3a186bd532ba`

addon64 SHA256:
`950264fae59ba7ae99ab7df35348eedfed126c6e49e3e8268aef063220aac4b4`

Supabase source artifacts:
- ZIP artifact ID `1392`
- addon64 artifact ID `1393`

Construction/build evidence:
`8038f026-6bc1-406a-80fa-eccf7b934aa2`

Canonical R36 build finding:
`renderer.pointlight.r36_source_cache_build_attested_v1`
finding ID:
`c2371d5e-337f-46bf-b868-3432b9ce3d58`

Promoted revision:
`11341`

---

## 4. How the PointLight bridge is actually structured

### Shader replacement timing

The PointLight replacement shader is create-time materialized/replaced.

It is NOT recreated per draw.

Conceptually:

`DSR creates exact known PointLight shader`
-> exact shader identity check
-> create-time replacement with audited DXBC
-> replacement pipeline object persists
-> game later binds/uses that pipeline normally.

Therefore, if performance collapses during gameplay, do not assume shader compilation/replacement is occurring per draw.

### Dynamic PointLight data timing

The PointLight source data is dynamic and is associated with draw/selector routing.

Current semantic chain:

`FLVER/material identity`
-> `clustered_pnts_selector_event_bridge`
-> source stage:
   `clustered_pnts_selector_source_event_bridge`
   -> `clustered_pnts_draw_runtime::selector_source_event()`
-> material-response stage:
   `clustered_pnts_selector_identity_event_bridge(identity, pointlight_spc)`
   -> `clustered_pnts_draw_runtime::selector_identity_event(...)`
-> prepared sidecar / GPU carrier
-> exact replacement shader
-> original/native draw path where possible
-> restore relevant D3D state.

R34 deliberately separated source production from material authorization.

R35 then allowed BOTH:
- NoSpc
- Spc

to consume the independent PTDE source stage.

R36 does not revert that semantic decision. It optimizes the source stage.

---

## 5. PTDE PointLight source carrier

### DirectPointLightEntity

Canonical finding:
`renderer.pointlight.direct_source_cpu_carrier_homology_v1`
revision `11294`.

Exact DSR retail source vfunc:
RVA `0x55C570`

PTDE homologue:
VA `0x00D34D50`

Confirmed equivalent emitted carrier:
`{ position.xyz, 1/(End-Begin), RGB/source signal, End }`

Near-zero fallback behavior is homologous.

R31 authorized exactly this Direct source class and no unknown classes.

R36 leaves DirectPointLightEntity on its exact homologous native packer.

Important:
R36 frame cache is currently for Bank/LerpBank source classes. Direct is not the principal cache target.

### BankPointLightEntity / LerpBankPointLightEntity

R36 uses existing PTDE donor/source reconstruction for these classes.

Attested DSR source packer addresses used in current runtime:
- Bank: base + `0x55BC00`
- LerpBank: base + `0x55D0B0`

Host-owned position lanes are preserved from the source object:
- Bank position.xyz: node + `0x60`
- LerpBank position.xyz: node + `0x70`

The PTDE donor reconstruction fills the remaining PTDE carrier semantics:
- invRange
- RGB/source signal
- End

If PTDE donor authority is unavailable, R36 preserves pre-R36 failover:
use the attested stock source packer rather than suppressing the light.

No arbitrary gain is permitted.

---

## 6. R32 performance localization — do not redo this work

Canonical:
`renderer.pointlight.performance_source_capture_nested_selector_hotpath_v1`
revision `11306`.

R32 sampled profiler proved the dominant clustered PointLight CPU cost was source capture nested in selector publication.

Authenticated timing:

- producer average: `1491.379 us`
- producer max: `4568.100 us`
- source capture on executing samples: `2307.744 us`
- first-four selection: `10.307 us`
- sidecar build: `0.157 us`
- draw-side prepare: `9.200 us`
- GPU cache: `0.300 us`
- upload: `3.500 us`
- whole synchronous draw transaction: `6.400 us`
  - begin/capture+mutate: ~`5.000 us`
  - restore: ~`1.200 us`

Meaning:
The expensive part was NOT:
- shader execution setup,
- sidecar build,
- upload,
- native draw transaction,
- restore.

It was synchronous source reconstruction / capture in the FLVER selector path.

Do not repeat old broad performance A/B tests to rediscover this.

---

## 7. R33 -> R34 -> R35 -> R36 evolution

### R33 — protected Spc fail-open

Build:
`pmetal_r33_clustered_spc_pre_source_failopen_0c90b9a9`
build ID `212`

R33 prevented clustered Spc from entering expensive source reconstruction.

Observed runtime:
Spc terminated at `cached_authority_reject` before source capture / sidecar / apply.

This removed the R32-identified expensive Spc route, but also meant Spc did not receive PTDE PointLight source.

Canonical:
`renderer.pointlight.r33_clustered_spc_runtime_failopen_verified_v1`
revision `11314`.

### R34 — source/material architectural split

Build:
`pmetal_r34_pointlight_source_material_split_0f946bbc`
build ID `214`

R34 separated:
- source stage: `selector_source_event()`
- material stage: `selector_identity_event()`

NoSpc used PTDE source.
Spc still failed open before source.

Canonical:
`renderer.pointlight.source_material_stage_split_v1`
revision `11321`.

### Owner authorization changed Spc policy

The owner explicitly authorized the hybrid:
`PTDE source/attenuation -> stock DSR Spc/GGX/Schlick tail`.

Old anti-hybrid PROTECT:
`protect.renderer.pointlight.clustered_spc_no_partial_legacy_specular_hybrid_v1`

Current state:
`protected = false`

Disabled at revision:
`11325`

Current allowed scope explicitly says:
`PTDE PointLight source/attenuation carrier -> surviving stock DSR Spc/GGX/Schlick material-response tail`

Required label:
`HYBRID_NOT_PTDE_LOCAL_SPEC_EQUIVALENT`

Pixel equivalence:
OPEN.

Do NOT accidentally re-enable the old R33 policy unless the owner changes scope again.

### R35 — Spc receives PTDE source

Build:
`pmetal_r35_pointlight_spc_hybrid_99c9087b`
build ID `215`

Source:
`99c9087b96511ac6e13ace54a19244987371ae76`

R35 routing:
`clustered_pnts_selector_source_event_bridge();`
then
`clustered_pnts_selector_identity_event_bridge(identity, pointlight_spc);`

Therefore both NoSpc and Spc consume PTDE source production.

Spc remains hybrid because local-specular tail is still DSR GGX/Schlick.

R35 runtime was authenticated:
clustered Spc reached native current draw apply:
`decision_active_cached`
-> `sidecar_ready`
-> `direct_current_native_applied`
-> `applied`

Owner then reported performance was severely degraded.

Canonical:
`renderer.pointlight.r35_spc_hybrid_runtime_perf_regression_v1`
revision `11336`.

Important interpretation:
Because the observed R35 Spc hit already reached `direct_current_native_applied`, the old deferred-context replay fallback is NOT the active bottleneck for that hit.

Combined with R32 timings, the R35 regression is attributed at high confidence to re-enabled synchronous PointLight source capture.

### R36 — source cache + duplicate pack removal

R36 keeps R35 semantics:
- NoSpc gets PTDE source.
- Spc gets PTDE source.
- Spc material tail remains stock DSR GGX/Schlick hybrid.

R36 changes performance architecture only.

---

## 8. Exact R36 performance changes

### A. Remove duplicate native pack on Bank/Lerp donor success

Before R36, Bank/Lerp source capture could effectively do:

`stock DSR source packer`
then
`PTDE donor reconstruction`

for the same light.

That meant paying for native packing and then replacing/reconstructing the carrier immediately afterward.

R36 changes this:

For Bank/Lerp:
1. Read only the host-owned position lanes directly.
2. Run PTDE donor reconstruction.
3. If PTDE donor succeeds, do NOT run stock packer.
4. If donor fails, fall back to stock packer.

Marker:
`[DSRRL POINTLIGHT R36] bank_lerp_native_double_pack=OFF frame_source_cache=TLS_EXACT_STATE_PER_PRESENT donor_miss_stock_fallback=ON`

### B. 32-entry TLS source carrier cache

R36 adds:
`k_frame_source_cache_entries = 32`

Scope:
thread-local.

Lifetime:
presented-frame epoch.

The cache is invalidated/reset by:
`clustered_pnts_draw_runtime::frame_event(frame_serial)`

and integrated runtime calls:
`g_clustered_pnts.frame_event(present);`

The cache is NOT a global stale light cache.

### C. Exact state snapshot before reuse

A Bank/Lerp cached carrier is reused only if the exact tracked source-object state still matches.

Current cache state includes:
- node pointer
- attested source vfunc target
- owner pointer
- source ID
- source category
- source class
- selector word 0
- selector word 1
- position bits

Therefore reuse is conditional on the source object remaining equivalent for the current presented-frame epoch.

Cache hit one-shot marker:
`[DSRRL POINTLIGHT R36] frame_source_cache_hit=1 exact_state_snapshot=ON`

Important:
This marker is logged once, not continuously.

Its presence proves at least one exact cache reuse occurred.

Its absence does NOT automatically prove R36 is broken. It can also mean:
- no repeated cacheable Bank/Lerp source occurred,
- only Direct sources were exercised,
- the exact state changed between accesses,
- the PointLight path was not exercised,
- a different thread did not encounter repeat state before the observed log ended.

Do not treat “no cache_hit line” alone as failure.

---

## 9. R36 startup markers that MUST authenticate the intended binary

The next log should first be authenticated against R36 build identity.

Expected inherited PointLight markers include:

`[DSRRL POINTLIGHT R34] source_material_split=ACTIVE source_stage=selector_source_event material_stage=selector_identity_event`

`[DSRRL POINTLIGHT R35] owner_authorized_spc_hybrid=ACTIVE clustered_nospc_consumes_ptde_source=ON clustered_spc_consumes_ptde_source=ON spc_material_tail=STOCK_DSR_GGX_SCHLICK local_specular_equivalence=OPEN`

`[DSRRL POINTLIGHT R36] bank_lerp_native_double_pack=OFF frame_source_cache=TLS_EXACT_STATE_PER_PRESENT donor_miss_stock_fallback=ON`

Also authenticate the build/source line:
- flavor should identify R36 PointLight source cache
- source commit must be `332e8bbfcac07d3448d0d1cdf6307aedfb9a0c07`

If source commit or flavor differs, DO NOT interpret the log as R36 evidence.

---

## 10. How to interpret the next R36 log

Always interpret in this order.

### Step 1 — Build authentication

Confirm:
- exact R36 addon loaded,
- source commit `332e8bbf...`,
- R34 source/material split marker,
- R35 owner-authorized Spc hybrid marker,
- R36 source-cache marker.

If these are absent or mismatch:
status = wrong build / unauthenticated log.
Do not draw performance conclusions about R36.

### Step 2 — Runtime liveness

Look for PointLight runtime telemetry / CLPNTS liveness.

A loaded addon is not enough.

If Material Response / EnvSpec activate but no PointLight event occurs, do not call PointLight broken; the captured session may simply not have exercised an authorized clustered PointLight receiver.

### Step 3 — Receiver/material gate

Useful PointLight stage markers include:

`cached_authority_reject`
`decision_active_cached`

Interpretation:

`decision_active_cached`
means the current exact material/receiver authority was accepted for PointLight processing.

`cached_authority_reject`
in R36 is NOT the old intentional R33 Spc anti-hybrid gate. The owner-authorized hybrid is now enabled.

If an expected Spc material now terminates at `cached_authority_reject`, investigate:
- owner identity,
- material identity,
- exact receiver,
- stale selector authority,
- material max,
- routing mismatch.

Do not “fix” it by globally enabling all similar shaders.

### Step 4 — Source stage

Look for:
- source capture success/fail counts in CLPNTS telemetry,
- one-shot source capture failure reason lines,
- R36 cache hit marker.

Failure line family:
`[DSRRL POINTLIGHT R29] source_capture_fail reason=...`

These old marker names can still appear because the failure logger predates R36.

Possible causes include:
- unattested source class,
- non-executable source vfunc,
- unreadable source state,
- non-finite carrier,
- invalid invRange,
- invalid End.

Do not infer a material-response bug from source-capture failure.

### Step 5 — R36 cache reuse

Preferred evidence:
`[DSRRL POINTLIGHT R36] frame_source_cache_hit=1 exact_state_snapshot=ON`

If present:
- R36 cache is live,
- exact Bank/Lerp state was reused,
- at least one repeated source avoided donor reconstruction.

If absent:
do not immediately conclude failure; correlate with source class and activation.

### Step 6 — Sidecar / draw apply

Healthy activation chain for an authorized hit should progress toward:

`decision_active_cached`
-> `sidecar_ready`
-> `batch_ready` / equivalent prepare success
-> `direct_current_native_applied`
-> `applied`

R35 already proved this native path can execute for Spc.

If R36 reaches `direct_current_native_applied`, do not blame old replay fallback for current cost without new evidence.

### Step 7 — Performance interpretation

Production R36 intentionally does NOT contain the old R32 sampled profiler.

Therefore:
- sparse timestamp gaps in ReShade.log are NOT callback duration measurements,
- a 500 ms or multi-second gap between two one-shot log lines does not prove either line blocked for that duration.

Use the runtime log to prove:
- build identity,
- route,
- activation,
- cache hit/fail behavior.

Use owner observation of FPS/stutter as direct runtime performance evidence when voluntarily provided.

Do not require a new controlled test as a solver prerequisite.

---

## 11. Decision tree for the next agent

### Case A — R36 loads, PointLight applies, cache hit observed, performance is restored

Interpretation:
- construction PASS remains valid,
- runtime liveness can be promoted PASS,
- PointLight bridge activation can be promoted PASS for the observed receiver/path,
- R36 cache mechanism is runtime-confirmed,
- owner performance regression from R35 is resolved for the observed scenario.

Do NOT promote pixel equivalence automatically.

Next technical work:
- visible PTDE PointLight behavior / remaining residual,
- or full PTDE local-specular Spc island if the owner wants to remove the hybrid.

### Case B — R36 loads, Spc applies, cache hit observed, performance is still bad

This is the most important remaining performance case.

Do NOT rerun old broad A/B bisects.

R36 would have proven:
- native draw path active,
- frame cache active,
- repeated Bank/Lerp captures are being avoided.

Next suspects should be narrow and evidence-driven:
1. first source capture cost once per unique light per frame is still too expensive,
2. donor authority/source-manager traversal inside first-use capture,
3. many unique Bank/Lerp lights defeat a 32-entry frame cache,
4. cache keys churn because tracked source state changes legitimately,
5. DirectPointLightEntity dominates and is not cached by R36,
6. another independent renderer operator is producing the user-observed performance loss.

Allowed next diagnostic:
narrow timing around surviving first-use/cold source capture and exact cache hit/miss accounting.

Preferred logging:
- sampled or thresholded,
- no per-frame spam,
- do not repeat the old broad bisect matrix.

### Case C — R36 loads, Spc applies, NO cache hit marker, performance is still bad

First determine why the cache is not being reused.

Do NOT immediately enlarge cache or weaken identity.

Inspect:
- are sources Bank/Lerp or Direct?
- do the same node/source IDs recur?
- does selector word / owner / position state churn?
- are calls distributed across different TLS threads?
- is each light only consumed once per frame?
- is 32-entry capacity actually exceeded?

If repeated Bank/Lerp identity is visible but cache misses anyway:
this is an R36 cache-key/epoch implementation problem.

If only Direct sources are active:
R36 Bank/Lerp cache is not expected to help; then Direct needs a separately justified performance design.

### Case D — R36 loads but expected Spc ends at `cached_authority_reject`

This is routing/authority, not performance.

Remember:
the old anti-hybrid Spc PROTECT is disabled.

Investigate exact:
- owner,
- material slot,
- receiver identity,
- current selector identity publication,
- material decision.

Fail open rather than guess.

### Case E — R36 shows `source_capture_fail`

Classify the reason.

If donor authority fails for an otherwise attested Bank/Lerp source, R36 should preserve stock-packer fallback.

If the source class/vfunc/state itself is unattested or invalid, fail-open is correct.

Do not force arbitrary source interpretation.

### Case F — R36 loads but no PointLight activation appears

Do not call the bridge dead if the session did not exercise the path.

Check whether:
- addon and PointLight runtime initialized,
- other independent operators activate,
- no authorized clustered PointLight draw occurred.

No new user runtime test may be made a blocking prerequisite under the global no-new-runtime-dependency rule.

### Case G — performance is good but visuals are wrong

Do NOT “fix” visible mismatch through performance code.

Separate:
- PointLight source semantics,
- attenuation,
- diffuse material response,
- Spc hybrid local-specular response,
- independent EnvSpec/cubemap response.

If only Spc highlight shape/energy is wrong while light range/color/diffuse are right, the likely remaining operator is the intentionally stock DSR GGX/Schlick local-specular tail.

That requires a real PTDE local-specular island, not source gain/exposure hacks.

---

## 12. Current Spc semantics — critical

Current R36 Spc is intentionally:

`PTDE PointLight source/carrier`
+
`DSR local specular material tail`

This means:
- PointLight position/source/range carrier can be PTDE,
- attenuation correction can be PTDE,
- but Spc BRDF/microfacet behavior is still DSR.

Do not conflate this with EnvSpec.

`PointLight Spc`
= material response to a local point light.

`EnvSpec`
= material response to environment/cubemap/probe.

They can both produce highlights on metal but are separate operators.

The long-term exact Spc target remains a complete PTDE local-specular kernel with its actual PTDE operands and downstream composition.

Historical anti-hybrid notes mention PTDE operands including:
`N, V, L / reflection, rdotl, c102, SpecRGB*c101*COLOR0`

but the current owner explicitly allows the hybrid until the full kernel exists.

Do not claim those full PTDE local-specular equations are already ported.

---

## 13. Current relevant PROTECT state

### Spc anti-hybrid
Key:
`protect.renderer.pointlight.clustered_spc_no_partial_legacy_specular_hybrid_v1`

Current:
DISABLED (`protected=false`)

Revision:
`11325`

Owner-authorized hybrid:
`PTDE PointLight source/attenuation -> stock DSR Spc/GGX/Schlick tail`

Pixel equivalence:
OPEN.

### Closed performance bisects
Key:
`renderer.runtime_v2.performance_bisect_closed_tests_do_not_repeat_v1`

Still ACTIVE.

Do not repeat without a material source-lineage change:
- ADDON_LOADED_ONLY
- DRAW_CALLBACK_ONLY
- POINTLIGHT_ONLY_BYPASS
- UL_H3_SUBSURFACE_PHYSICAL_CUT
- IDENTITY_VS_RESOURCE_SPLIT
- FLVER_VS_TEXTURE_SPLIT
- SELECTOR_VS_SUPPORT_SPLIT
- OLD_REPLAY_FAMILY_LOCALIZATION

### No duplicate performance bisects
Key:
`renderer.runtime_v2.no_duplicate_performance_bisects_without_lineage_change_v1`

Still ACTIVE.

Prefer direct/narrow instrumentation over another binary A/B for already-tested boundaries.

### No new capture/runtime dependency
Key:
`protect.project.no_new_capture_or_runtime_test_dependency`

Still ACTIVE.

A voluntarily supplied log can and should be consumed.
Do not make a new user test/capture a prerequisite to continue static/RE work.

---

## 14. Independent renderer state that should not be accidentally changed while fixing PointLight

Current P_Metal lineage keeps:
- HemDir3 physically cut/OFF
- Subsurface physically cut/OFF
- Upper/Lower physically cut/OFF

These systems must not be re-enabled as part of a PointLight performance fix.

Material Response and EnvSpec are independent renderer paths and can remain active.

Do not repair:
- PointLight with Bloom,
- PointLight with exposure,
- PointLight range with arbitrary intensity,
- Spc with global LightBank gain,
- EnvSpec with PointLight changes.

Operator isolation remains mandatory.

---

## 15. SpecRGB scope reminder

SpecRGB sidecar scope remains equipment-only unless separately proven.

PROTECT:
`protect.renderer.specrgb.equipment_only_sidecar_scope`

Do not let clustered PointLight automatically inherit equipment SpecRGB sidecars merely because Spc is active.

Current owner-authorized Spc hybrid does NOT itself authorize PTDE SpecRGB feeding the surviving DSR PointLight GGX/Schlick tail.

That would require a separate exact consumer proof / scope change.

---

## 16. What NOT to do next

Do NOT:
- revert all PointLight to stock DSR just because performance was bad in R35;
- reintroduce R33 Spc fail-open unless owner changes scope;
- recreate shaders per draw;
- assume a ReShade log timestamp gap is execution duration;
- repeat the closed broad perf A/B tests;
- blame replay fallback if `direct_current_native_applied` is observed;
- globally widen receiver matching;
- disable unrelated DSR systems;
- use arbitrary intensity/specular gains;
- treat cache hit as pixel equivalence;
- treat build PASS as runtime PASS;
- treat runtime PASS as pixel PASS;
- require new screenshots/runtime tests to continue static analysis.

---

## 17. If R36 performance still fails: preferred next engineering direction

The next agent should work from the actual R36 outcome.

If cache reuse is confirmed but performance remains poor, the most productive next cut is likely to move PointLight source preparation even farther away from material draw frequency:

Desired architecture:

create-time:
`replacement shader`

source update / frame producer:
`resolve PTDE PointLight source carrier once per changed/visible source`

draw:
`select already-prepared carrier -> bind -> native draw`

The semantic goal is not “cache harder at draw time”; it is:
source production frequency should follow source-state changes, not material draw count.

However, do not implement a global persistent cache without proving:
- object lifetime,
- source identity,
- source update timing,
- map/load invalidation,
- thread ownership,
- source-manager mutation semantics.

R36 is intentionally conservative: TLS + exact state + per-present epoch.

---

## 18. Relevant source files

Primary PointLight runtime:
`renderer-core/src/runtime/clustered_pnts_draw_runtime.cpp`

Runtime header:
`renderer-core/include/dsrrl/runtime/clustered_pnts_draw_runtime.hpp`

FLVER selector dispatch:
`renderer-core/src/runtime/flver_engine_hooks.cpp`

Integrated addon / pipeline and draw transaction:
`renderer-core/integrated/integrated_addon.cpp`

PTDE donor/source logic:
`renderer-core/include/dsrrl/runtime/pointlight_ptde_source.hpp`
`renderer-core/include/dsrrl/runtime/pointlight_ptde_source_runtime.hpp`

Clustered PointLight materializer:
`renderer-core/src/operators/point_light/clustered_pnts_direct_materializer.cpp`

Audit:
`renderer-core/tools/audit_active_islands_hotpath.py`

Audit:
`renderer-core/tools/audit_clustered_pnts_activation_sources.py`

Build workflow:
`.github/workflows/pmetal-r19-lerp-exact-envdiffuse-fast.yml`

---

## 19. Current important canonical keys / revisions

`renderer.pointlight.direct_source_cpu_carrier_homology_v1`
CONFIRMED
rev `11294`

`renderer.pointlight.performance_source_capture_nested_selector_hotpath_v1`
CONFIRMED
rev `11306`

`renderer.pointlight.r33_clustered_spc_runtime_failopen_verified_v1`
CONFIRMED
rev `11314`

`renderer.pointlight.source_material_stage_split_v1`
CONFIRMED
rev `11321`

Spc anti-hybrid PROTECT disabled:
rev `11325`

`renderer.pointlight.r35_spc_hybrid_runtime_perf_regression_v1`
CONFIRMED
rev `11336`

`renderer.pointlight.r36_source_cache_build_attested_v1`
CONFIRMED
rev `11341`

At handoff creation, latest revision to re-check is at least:
`11341`

The next agent MUST query current revision rather than assuming this remains latest.

---

## 20. R35 authenticated runtime evidence

Previous R35 runtime log source artifact:
`1390`

SHA256:
`fbbbc9db92b24cbaecb21ebb3e2b23da9b6942b11d5ad2bbef7fef565309dd60`

Evidence:
`c7b37b82-b43e-4530-89b2-4fad28a2b1a6`

Observed:
- exact R35 hybrid marker active,
- clustered Spc active,
- `decision_active_cached`,
- sidecar ready,
- native current draw apply,
- final applied path,
- owner reports severe performance regression.

This is the baseline R36 is intended to improve.

Do not interpret R36 without comparing its route to this baseline.

---

## 21. Expected status promotion from a good R36 log

If the next authenticated R36 log proves:
- correct build,
- PointLight liveness,
- intended Spc/NoSpc activation,
- R36 cache hit or otherwise valid source path,
- native draw apply,

then update R36 control-plane metadata accordingly.

Possible status:
- construction: PASS
- native build: PASS
- runtime liveness: PASS
- bridge activation: PASS for observed route
- performance: only PASS if direct owner/runtime evidence supports recovery
- pixel: remain OPEN unless visible PTDE behavior is actually validated

Do not promote pixel status from telemetry alone.

---

## 22. One-paragraph takeover summary

We are restoring PTDE PointLight source behavior inside DSR. Shader replacement is create-time; dynamic PointLight carrier data is routed through a source stage and a separate material-response stage. The owner explicitly allows clustered Spc to be a hybrid: PTDE PointLight source/attenuation feeding the surviving DSR GGX/Schlick local-specular tail. R35 proved this Spc route activates via the native current draw path but caused severe performance loss. R32 had already localized the dominant CPU cost to synchronous PointLight source capture (~2.3 ms on executing samples), not selection/upload/draw transaction. R36 keeps the R35 hybrid semantics but removes duplicate Bank/Lerp native packing when PTDE donor capture succeeds and adds a 32-entry TLS, exact-state, per-present source-carrier cache. R36 build 216 / source `332e8bbf...` is construction/native-build PASS, but runtime/performance/pixels are still OPEN. The next log must first authenticate R36, then check PointLight authority/apply chain and the one-shot `frame_source_cache_hit=1 exact_state_snapshot=ON` marker. If cache hits and performance is still bad, do not repeat broad bisects: instrument only surviving cold/first-use source capture or identify Direct-source dominance / high unique-light churn. If performance is fixed, promote runtime/performance separately but keep pixel equivalence OPEN until visible PTDE behavior is proven.
