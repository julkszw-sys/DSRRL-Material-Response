# DSRRL — PointLight R20 Performance Full Handoff

Date: 2026-10-05
Repo: `julkszw-sys/DSRRL-Material-Response`
Active PR: `#245`
Branch: `r19-lerp-exact-envdiffuse-ab-beta`
Authoritative source head at handoff: `327764c5b3f868cfe905c09642ec9c700efefb36`
Supabase current revision at runtime-falsifier stage: `11220`

## 1. Goal / scope

This handoff is ONLY for the current Renderer Edition PointLight performance problem and the exact state required to continue it safely.

Project target remains PTDE-visible PointLight behavior on DSR host renderer. Do not redefine success as raw PARAM equality, first-consumer equality, or build/runtime activation. Preserve unrelated DSR systems. U/L, HemDir3 and Subsurface are physically cut in the current P_Metal lineage.

The immediate task is now:

> Remove the remaining clustered PointLight performance regression without changing the confirmed PTDE PointLight equations or broadening receiver/material authority.

The performance problem is NOT solved in R20.

## 2. Mandatory next-agent bootstrap

Before changing code:

1. `dsrrl.agent_bootstrap('PointLight R20 deferred-context replay performance continuation')`
2. `dsrrl.renderer_addon_control_plane_status('pmetal_r20_pointlight_producer_cache_327764c')`
3. Verify PR245 head is still the expected branch/head before editing.
4. Read canonical finding:
   - `renderer.pointlight.r20_deferred_context_replay_fallback_perf_v1` rev 11220
5. Read canonical architecture finding:
   - `renderer.pointlight.r20_producer_cache_architecture_v1` rev 11213
6. Do not repeat closed performance A/B tests unless the relevant architecture materially changes.

## 3. Exact tested build

Build key:
`pmetal_r20_pointlight_producer_cache_327764c`

Build id:
`195`

Display:
`P_Metal R20 — PointLight Producer Cache`

Source:
`327764c5b3f868cfe905c09642ec9c700efefb36`

ReShade:
- version `6.8.0.1`
- API `20`
- pinned commit `3645e3025d1d98a90e318278858931f034d5d1f6`

Addon SHA-256:
`68be3b41682f8975f831e42e425d031eb29cde43a65786cb08a9144c73d1de27`

Addon size:
`1,935,360` bytes

Canonical build metadata currently records:
- workflow run `37243803302`
- artifact id `11318716309`
- wrapper ZIP SHA `a35bac62034df65fe28741fe380e9ee37ce988282a635e816aa1dc9fe775e517`

A duplicate successful workflow run `37243805128` produced artifact `11318427059`; that is the ZIP delivered to the owner. Its wrapper archive digest differs because it is a separate archive instance, but the contained addon has the SAME addon SHA above.

Delivered owner ZIP name:
`DSRRL_R20_POINTLIGHT_PRODUCER_CACHE_ADDON_ONLY.zip`

## 4. Owner runtime evidence

Owner log:
`ReShade(20261004-233551).log`

Runtime build authentication:
- addon loaded as `DSRRL_PMETAL_R20_POINTLIGHT_PRODUCER_CACHE.addon64`
- source commit logged as exact `327764c5...`
- flavor logged as `pmetal_r20_pointlight_producer_cache_ptde_source_cached`
- ReShade API 20

R20 startup marker proves new architecture is live:
`selector_authority_cache=ACTIVE source_selection_cache=PER_PRODUCER_SERIAL gpu_payload_dedupe=PER_CONTEXT draw_cpu_traversal=OFF`

Native bridge reports globally ACTIVE at startup.

The game creates several D3D11 deferred contexts before gameplay.

Observed active clustered PointLight path:
- first one-shot reject from missing cached authority occurs earlier and fail-opens normally
- later exact clustered Spc authority succeeds
- `decision_active_cached`
- `clustered_spc_legacy_specular_current_b12_ready`
- `shader_ready`
- `sidecar_ready`
- `batch_ready`
- **`stage=applied`**
- first active hit is on RT `480x270 fmt=10 viewport=480x270`

Critically there is NO:
`[DSRRL POINTLIGHT APPLY] stage=native_applied`

Owner result:
**performance still poor / FAIL_OWNER**.

This is already stored in Supabase as runtime evidence:
`ae68d1b5-3de1-473b-8ead-142a7b0ed2c3`

Canonical runtime finding:
`renderer.pointlight.r20_deferred_context_replay_fallback_perf_v1`
Finding id:
`fd1a3e36-4b73-4339-943a-18df4f422f58`
Revision:
`11220`

## 5. What R20 successfully changed

R20 is not a no-op. The following architecture is construction-confirmed and should NOT be reverted casually.

### 5.1 Clustered source / membership work moved off draw prepare

Before R20, each authorized clustered draw did approximately:

`draw -> material join -> select_first_four_exact -> light-list traversal -> source vfunc -> sidecar build -> t18/t19/b12 upload -> draw replay -> restore`

R20 now performs expensive CPU work at producer/selector authority and caches it:

`builder snapshot -> exact PointLight material prefilter -> producer-serial first-four + PTDE source capture -> cached sidecar`

Then draw preparation consumes cached authority/payload.

Current code:
`renderer-core/src/runtime/clustered_pnts_draw_runtime.cpp`

Important functions:
- `selector_identity_event(...)`
- `current_draw_authority(...)`
- `prepare_sidecar(...)`

`prepare_sidecar()` no longer calls:
- `select_first_four_exact()`
- `capture_source()`
- `build_clustered_sidecar_v1()`

### 5.2 Material authority prefilter moved upstream

Current selector cut:
`renderer-core/src/runtime/flver_engine_hooks.cpp`

`publish_exact_selector_identity(...)` now gates clustered PointLight using:
`direct_pointlight_material_candidate(identity, pointlight_spc)`

Only exact PointLight material authority enters clustered selector runtime.

### 5.3 Cached authority has freshness guard

`current_draw_authority(...)` rejects stale cache unless current producer TLS is valid and cached `serial + owner` match the current producer input.

Do not remove this to gain performance. It prevents stale payload application.

### 5.4 GPU payload upload dedupe

R20 uses per-context/generation TLS byte identity for `t18/t19/b12` and avoids identical uploads.

There is exactly one synchronized per-context GPU carrier lookup per prepare; Map/Unmap is outside the global resource mutex.

### 5.5 PointLight bank signature scan was linearized

Exact structural authority is preserved, but repeated per-byte readable-region lookups were removed from bank row-name hashing.

File:
`renderer-core/include/dsrrl/runtime/pointlight_ptde_source_runtime.hpp`

### 5.6 Native original-draw bridge was extended to PointLight

R20 added PointLight ownership to:
`renderer-core/src/runtime/pmetal_native_draw_bridge.cpp`

Integrated draw path attempts:
`g_pmetal_native_draw.arm_draw(...)`
or
`g_pmetal_native_draw.arm_draw_indexed(...)`

before replay.

If arming succeeds, addon returns `false` and lets the original D3D11 Draw execute once under the temporary mutation, then native hook restores state.

This is the intended fast path.

## 6. CONFIRMED remaining performance root cause

The intended native original-draw optimization is currently restricted to the ONE stored immediate D3D11 context.

### 6.1 Current install restriction

File:
`renderer-core/src/runtime/pmetal_native_draw_bridge.cpp`

`install(device)` does:

- `native_device->GetImmediateContext(&context)`
- requires `context->GetType() == D3D11_DEVICE_CONTEXT_IMMEDIATE`
- stores this single pointer as `impl_->context`
- patches vtable slots based on this context

### 6.2 Current arm restriction

Both `arm_draw()` and `arm_draw_indexed()` reject unless:

- `context != nullptr`
- `context == impl_->context`
- `context->GetType() == D3D11_DEVICE_CONTEXT_IMMEDIATE`

Therefore a ReShade draw callback running on a deferred D3D11 recording context cannot arm native original-draw.

### 6.3 Runtime proves this matters

The owner log creates several deferred contexts, and the real clustered PointLight activation ends at `stage=applied`, not `stage=native_applied`.

In current integrated code, successful native arm returns BEFORE the shared replay dispatch/accounting path. Therefore seeing `stage=applied` for this PointLight draw is direct evidence that native arm was not used and the draw fell through to shared replay transaction.

Conclusion:

**R20 producer/source/material caching is live, but the tested PointLight draw still pays the draw replay transaction because the real DSR render path uses a deferred context.**

This is the current highest-confidence performance bottleneck.

## 7. Exact current native bridge mechanics

File:
`renderer-core/src/runtime/pmetal_native_draw_bridge.cpp`

Important state:
- thread-local `pending_draw g_pending`
- `pending_draw` already stores the actual `ID3D11DeviceContext *context`
- native hook callbacks call `pending_matches(...)`, which ALREADY compares pending context to the context passed to the actual hooked Draw
- native hook then:
  1. captures PS/CB/SRV/sampler state
  2. applies mutation
  3. calls original Draw once
  4. restores captured state
  5. logs `stage=native_applied` for PointLight on successful restore

This means the hook body itself is conceptually context-aware. The current hard blocker is primarily registration/arming/context1 ownership being designed around one immediate context.

Do not assume the immediate-context vtable hook automatically covers deferred contexts. Verify exact vtable identity or install per command list/context.

## 8. Best next architectural direction

### Preferred Route A: deferred-context native original-draw support

This is the narrowest continuation of R20 and should be attempted first.

Pinned ReShade API 20 already exposes:
- `addon_event::init_command_list`
- `addon_event::destroy_command_list`

Pinned ReShade D3D11 implementation invokes `init_command_list` for each D3D11 device context; only immediate context additionally acts as command queue.

Potential implementation shape:

1. Register `init_command_list` / `destroy_command_list` in integrated addon.
2. For D3D11 command lists, obtain native `ID3D11DeviceContext *` from `cmd_list->get_native()`.
3. Track exact context identity and `GetType()`.
4. Build a context registry for immediate + deferred contexts.
5. Acquire/retain `ID3D11DeviceContext1` per context if needed for CB window capture/restore.
6. Verify whether deferred and immediate contexts share the same vtable slots. If not, install exact Draw/DrawIndexed/Instanced hooks per distinct vtable with safe reference counting/lifetime.
7. Change `arm_draw*()` from `context == impl_->context && immediate` to `registered exact D3D11 context with matching hook/context state`.
8. Preserve `pending.context == actual hook context` matching.
9. Preserve full PS/CB/SRV/sampler capture/restore and quarantine on restore failure.
10. Fail-open if context registration/type/vtable/context1 is unknown.

Do not broaden to arbitrary context types.

### Important deferred-context semantic concern

On a D3D11 deferred context, Draw does not execute immediately on GPU; it records commands into a command list. That is fine in principle if the bridge mutates/restores state on the SAME deferred recording context around the original Draw call. The recorded command stream should then contain the mutation, draw, and restore in order.

But this must be proven with exact D3D11/ReShade hook behavior. Do not assume an immediate-context state bridge can simply be reused by pointer relaxation.

### Preferred Route B if Route A is structurally wrong

Move PointLight state mutation even earlier so draw-time transaction disappears:

- exact material/selector semantic cut decides authority
- dynamic PTDE carriers are materialized during producer/resource update
- bind replacement PS / CB / SRV at the exact material or pipeline semantic boundary
- restore/replace on the next exact material/pipeline transition rather than around every draw

This is more RenoDX/Special-K-like but is a bigger state-lifetime change.

Do NOT permanently create-time replace the shared clustered PntS shader solely by shader hash. Current canonical R20 architecture explicitly rejects that as unsafe because shader identity alone is not equivalent to material authority; the same shader family can service draws that must remain stock DSR fail-open.

## 9. What NOT to do next

Do not repeat these as primary fixes:

- turn telemetry off again — already falsified on R19 `9a3756f`
- add another generic early draw gate — already insufficient
- move `select_first_four_exact()` back into draw
- move source vfunc capture back into draw
- add arbitrary gain/intensity/range compensation
- disable PointLight math to gain FPS
- globally replace all PntS shaders by hash without exact material authority
- broaden to foreign source classes
- weaken fail-open or cached authority freshness
- attribute performance failure to PTDE PointLight shader math without profiler/evidence

Performance failure persisted after hot telemetry OFF and after R20 removed CPU selector/source traversal from draw, so the remaining replay/deferred transaction path has priority.

## 10. Runtime statuses at handoff

### Construction
PASS

### Compatibility / audits
PASS for R20 dedicated build and PointLight architecture/source/hot-path audits.

### Runtime liveness
PASS

### Bridge activation
PASS **via replay fallback**

### Native original-draw PointLight activation
NOT OBSERVED

### Performance
FAIL_OWNER

### Pixel behavior
OPEN

Do not promote pixel equivalence from activation.

## 11. PointLight correctness residuals unrelated to this performance bug

These remain important and must not be lost during performance work.

### 11.1 Clustered source payload authority split

Canonical:
`renderer.pointlight.current_source_payload_authority_split_v1` rev 11205

Current clustered Bank/LerpBank behavior:
- exact donor bank hit -> PTDE Begin/End/RGB/Intensity payload, Lerp A/B+beta preserved
- donor structural miss -> current code may retain DSR host raw source while PTDE operator still runs
- foreign source class -> fail-open stock DSR

The donor-miss path is a hybrid residual. Performance work must not normalize this into “pixel complete”.

### 11.2 Fixed PntSS/PntSSSS source values

Current fixed path captures DSR fixed-uploader raw payload and does not use the PTDE donor override. Thus fixed source numerics remain DSR unless host PointLightBank PARAM is replaced upstream.

Do not blindly inject clustered donor `q` into fixed path; fixed uploader/category transport semantics must be recovered first.

### 11.3 Selector / membership residual

Even exact PTDE donor rows do not prove PTDE selector/culling membership equivalence. Earlier canonical source contract explicitly keeps this as a separate residual, especially where PTDE End differs from DSR.

## 12. Historical PointLight lineage the next agent must know

### R19 full36 b12 repair

Canonical:
`renderer.pointlight.clustered_pnts_b12_abi_drift_full36_v1` rev 11180

Old replacement shaders expected historical b12 layout.
Current Runtime-v2 layout:
- `b12[0].w` = c102
- `b12[1].xyz` = c100
- `b12[2].xyz` = raw c101

Full36 migration repaired:
- all 36 c100 reads to `b12[1].xyz`
- 24 Spc c101 reads to `b12[2].xyz`
- c102 remains `b12[0].w`

Construction canonical:
`renderer.pointlight.clustered_pnts_full36_current_b12_migration_construction_v1` rev 11195

Build `b9d7431` activated PointLight but owner performance was bad.

### R19 performance candidate

Build:
`pmetal_r19_pointlight_perf_9a3756f`

Changes:
- telemetry default OFF
- early inactive direct-PointLight gate
- fixed selector no-snapshot fast return

Owner performance still bad.

Canonical rejection:
`renderer.pointlight.performance_fast_gate_candidate_runtime_rejected_v1` rev 11208

This is why R20 moved CPU work out of draw.

### R20

Architecture canonical:
`renderer.pointlight.r20_producer_cache_architecture_v1` rev 11213

Runtime performance falsifier:
`renderer.pointlight.r20_deferred_context_replay_fallback_perf_v1` rev 11220

## 13. Key R20 commits from 9a3756f -> 327764c

- `25d48a54` expose static PointLight material resolver
- `dec9eea1` keep resolver registry-free
- `88bb2af1` cached clustered PointLight authority API
- `b23cbf15` producer/upload caches
- `03310bcf` precompute source sidecar at selector authority
- `23ddf763` remove selector/source traversal from draw prepare
- `ef9bda7b` GPU payload upload dedupe
- `748e1ac2` selector-cached authority for draws
- `5e3da9df` cheap PointLight material prefilter
- `64663ea1` gate clustered selector runtime by exact PointLight material
- `03f764a9` resolve material once per selector
- `0faa2a1e` linearize PointLight bank signature scan
- `9c16020d` allow PointLight on native original-draw bridge
- `fe668176` route clustered PointLight through native original draw
- `4bb32315` lock-free upload dedupe after carrier lookup
- `70a4518e` reject stale cache by producer serial
- `a9b3e329` native PointLight execution marker
- `d6f663b1` audit PointLight-prefiltered selector dispatch
- `327764c5` align PointLight source audit with full36 Spc attestation

## 14. Key source files

Primary performance path:
- `renderer-core/src/runtime/pmetal_native_draw_bridge.cpp`
- `renderer-core/include/dsrrl/runtime/pmetal_native_draw_bridge.hpp`
- `renderer-core/integrated/integrated_addon.cpp`

Clustered producer/cache/carrier:
- `renderer-core/src/runtime/clustered_pnts_draw_runtime.cpp`
- `renderer-core/include/dsrrl/runtime/clustered_pnts_draw_runtime.hpp`
- `renderer-core/src/runtime/clustered_pnts_pipeline_runtime.cpp`
- `renderer-core/include/dsrrl/runtime/clustered_pnts_pipeline_runtime.hpp`

Semantic material cut:
- `renderer-core/src/runtime/flver_engine_hooks.cpp`
- `renderer-core/src/operators/material_response/material_response_island.cpp`
- `renderer-core/include/dsrrl/operators/material_response/material_response_island.hpp`

PTDE source donor:
- `renderer-core/include/dsrrl/runtime/pointlight_ptde_source_runtime.hpp`
- `renderer-core/include/dsrrl/runtime/pointlight_ptde_source.hpp`
- `renderer-core/include/dsrrl/runtime/pointlight_bank_structure_authority_v1.hpp`
- generated donor corpus header

Fixed path:
- `renderer-core/src/runtime/fixed_pointlight_draw_runtime.cpp`

Direct PTDE clustered shader provenance:
- `renderer-core/data/provenance/clustered_pnts_direct_stock_journal_v1.json`

Audits:
- `renderer-core/tools/audit_clustered_pnts_activation_sources.py`
- `renderer-core/tools/audit_active_islands_hotpath.py`
- `renderer-core/tools/audit_pointlight_ptde_source_runtime.py`

Build workflow:
- `.github/workflows/pmetal-r19-lerp-exact-envdiffuse-fast.yml`
  (name is historical; it builds R20 flavor on current branch)

## 15. R20 build/audit result

Dedicated Windows addon build succeeded on exact head `327764c`.

Passed:
- Active-islands hot-path audit
- Clustered PntS pre-runtime activation source audit
- PointLight core source audit
- PointLight material authority header audit
- binary self-identity / artifact upload

Do not treat CI success as runtime performance success.

## 16. Recommended next code milestone

Suggested name:
`R21 PointLight Deferred Native Draw`

Success construction criteria:

1. exact D3D11 command-list context registry exists
2. immediate + deferred context identity is explicit
3. native bridge can arm on exact registered deferred context
4. actual hook callback matches the same deferred context
5. mutation/state capture/restore occurs on that same context
6. unknown context/vtable/type fail-opens
7. no double Draw / no ReShade replay for successfully armed PointLight draw
8. no global shared-state mutation leakage
9. U/L/H3/Subsurface remain physically cut
10. PointLight equations and full36 b12 replacement unchanged

Runtime acceptance marker:

For the same active clustered PointLight path, the log must show:
`[DSRRL POINTLIGHT APPLY] stage=native_applied`

and must NOT rely on the replay `stage=applied` path for that same draw.

Performance acceptance:
Owner reports the PointLight-area FPS regression materially recovered relative to R19/R20 replay builds.

Pixel acceptance remains separate and OPEN until visually validated.

## 17. If R21 native-deferred still performs badly

Then do NOT go back to selector micro-optimizations.

The next target becomes eliminating per-draw PS/CB/SRV mutation entirely by moving the semantic cut to material/pipeline/resource lifetime:

`exact material authority -> prepare/bind PTDE state -> normal deferred Draw recording`

The key question would become whether exact material binding lifetime can safely carry replacement shader + b12/t18/t19 until the next material transition, while preserving stock fail-open for unauthorized draws.

A profiler/timing instrument on the architectural stages is preferable to another broad A/B binary set.

## 18. Supabase keys / evidence IDs

Use these directly rather than reconstructing history.

R20 architecture:
- key `renderer.pointlight.r20_producer_cache_architecture_v1`
- finding `f3842668-3198-4370-b33e-6c074c32c459`
- revision `11213`

R20 runtime perf falsifier:
- key `renderer.pointlight.r20_deferred_context_replay_fallback_perf_v1`
- finding `fd1a3e36-4b73-4339-943a-18df4f422f58`
- evidence `ae68d1b5-3de1-473b-8ead-142a7b0ed2c3`
- revision `11220`

Current source payload split:
- key `renderer.pointlight.current_source_payload_authority_split_v1`
- revision `11205`

R19 fast-gate perf rejection:
- key `renderer.pointlight.performance_fast_gate_candidate_runtime_rejected_v1`
- revision `11208`

Full36 b12 drift:
- key `renderer.pointlight.clustered_pnts_b12_abi_drift_full36_v1`
- revision `11180`

Full36 migration construction:
- key `renderer.pointlight.clustered_pnts_full36_current_b12_migration_construction_v1`
- revision `11195`

PTDE donor source contract:
- key `renderer.pointlight.selected_ptde_bank_donor_source_contract_v1`
- revision `10332`

Raw carrier homology:
- key `renderer.cross.pointlight.clustered_raw_geometry_carrier_homology_v1`
- revision `9833`

## 19. PROTECT / constraints relevant to continuation

- Do not create partial PTDE/DSR hybrid local-specular path.
- Preserve exact receiver/material authority and fail-open.
- Do not broaden PointLight to similar shaders without census/attestation.
- Do not treat build/runtime PASS as pixel PASS.
- Do not fix PointLight performance by altering PTDE operator math.
- Do not compensate source/range/color with arbitrary intensity multipliers.
- Do not re-enable Upper/Lower, HemDir3 or Subsurface in this lineage.
- Do not repeat already closed broad Runtime-v2 performance bisects without a material architecture change.

## 20. Final handoff status

R20 solved much of the CPU-side draw hot path but did NOT solve owner performance.

The current performance problem is now narrowed to this exact architectural fact:

> The actual clustered PointLight draw is recorded through a D3D11 deferred context, while the R20 native original-draw bridge accepts only one stored immediate context. The arm therefore fails and integrated runtime falls back to the expensive shared replay transaction.

That is the first thing the next agent should fix.

Do not restart the investigation from telemetry, selector traversal, b12 ABI, or donor decoding. Those areas have already been materially addressed and/or independently validated.
