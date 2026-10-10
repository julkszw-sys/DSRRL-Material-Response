# v2.0.3 PR300: performance, HemEnvLerp, synthetic m10/row0 and EnvDiffuse

**Date:** 2026-10-11  
**Baseline:** PR300, exact commit `6381743f5e5f82c75bd7e5b7ea6d0f7675e62eea`.  
**Scope:** read-only source/RE audit. No executable patch, shader change, producer change, renderer hook, extra resource allocation, runtime telemetry or change to protected PointLight/DoF.  
**Target:** operator-local PTDE-visible behavior. Runtime/Pixels on the 24 new Leather/DullLeather tuples **OPEN**.

## Authority and known measurements

The previously measured integrated draw callback (~0.497 us mean on an authenticated *fast launch*) and FLVER selector body (~0.793 us mean / 14.7 us maximum, 1/1024 samples in an earlier source lineage) were **not measured on PR300**. R52/current-main evidence also observed 25–30 FPS versus 50–60 FPS between process restarts with the *same addon*, even when observed regular selector/draw work was small. The existing performance no-repeat PROTECT rules close the previous broad PointLight, FLVER and full-hot-telemetry A/B bisections. Therefore **no honest current PR300 fps attribution is possible from these old numbers**; a replacement profiler must compare identical binaries, an authenticated slow/fast launch, and include the support-hook/CPU scheduling frontier.

Do not confuse construction PASS (CI 38092023277, 5/5 native tests, one addon SHA256 `f9d6aa05c37415b26b7b4647c8d788d84ba9e5999050082f1923b0cb1ef1073c`) with measured performance, bridge runtime, or pixel behavior. PR300 remains draft/diagnostic.

## Exact material and operator boundaries

The attested sample has 36 FLVER SHA256+slot+raw-MTD+logical specular tuples, of which 12 are `P_Metal[DSB].mtd` (original island) and **24 are nonmetal** (12 `P_DullLeather[DSB].mtd`, 12 `P_Leather[DSB].mtd`). The source's generated raw MTD router specifies EnvSpec slots DullLeather=0, Leather=1, Metal=2. Never set slot=2 for both leather materials or infer MTD from RX alone.

PR300 new nonmetal gate requires exact provenance, `DifSpcBmp`, MTD EnvSpec PRESENT, rx33..35, finite nonnegative c101 and current PS t1 matched to that slot's logical gSpec with PTDE t10 sidecar. The dedicated EnvSpec specular resource path avoids the *generic* MR SpecRGB census veto only after these checks. Other materials retain stock DSR fallback. The original global MR call still passes `spec_rgb_consumer_ready=false`, preventing an unrelated DSR PBL/F0 tail from inheriting PTDE RGB.

The shader itself is currently reused from P_Metal; **this does not prove identical PTDE response for leather**, even if t10 transport activates. Receiver-families and material operators must be compared independently, and the 12 original metal rows must not be rerouted through the new nonmetal gate.

## CPU performance frontier inventory

| Boundary | Source work that actually executes | Frequency / risk | Diagnostic, not mutation |
|---|---|---|---|
| FLVER selector/ownership | exact material selection, 36-entry provenance lookup, source callback; producer key includes full FLVER SHA, material slot and MTD | Per eligible selector. Source authority is high-risk; wrong origin changes lighting | Report selector samples separated by exact material and return RVA; no forced bank |
| Raw-MTD semantic predicate | `classify_mtd_envspec_semantics` iterates up to 325 generated records; `exact_proven_owner_slot` scans 36 | Reused across candidate, source and draw; potentially redundant CPU reads | Isolate exclusive gate microseconds and counts; never optimize by name-only hash |
| P_Metal source transport reused by nonmetal | authenticated descriptors, LightBank A/B, `pmetal_producer_state` TLS + 256-key synchronized slots | Cross-thread deferred-context handoff and mutex fallback; source state cannot be borrowed | Count exact TLS hits, keyed hits, misses, lock contention without changing epoch |
| Material companion | `inspect_many` queries diffuse/spec/normal companion caches, then exact 36-row stock gSpec identity | Per activated candidate; must keep roughness/alpha in stock DSR t1 | Count cache hits, slow misses, extra PSGetShaderResources fallback |
| Native EnvSpec probes | t12/t14 read, view→probe map, sampler/COM reference traffic and mutex | Per candidate draw; native SRV lifetime and probe routing safety-critical | Distinguish cached view/map lookups from no-identity fail-open |
| PTDE EnvDiffuse | existing probe-ordinal PTDE RGBA pack/cube A/B, get-or-create then AddRef/Release | Cold GPU resource materialization may hitch; warm cached lookup may still lock | Count first use per probe, subsequent hits, and rejected missing A/B |
| b12 upload | per-context cached D3D11 dynamic constant buffer, `Map(WRITE_DISCARD)` + Unmap | Per accepted draw; potential driver synchronization (unmeasured) | Profile Map and upload separately, by context and stable/Lerp |
| Draw transaction | shader, constant buffers, t10/t11/t13/t12/t14, sampler capture/bind/restore | Per activated draw, receiver family and material identity | Profile begin/capture/mutate/restore, not an arbitrary renderer-wide bypass |

**No measured PR300 cost or speedup is claimed.** The 36/325 values are source-table dimensions, *not* observed per-frame operation counts. A new immutable index or per-epoch decision cache is a hypothesis only until equivalent miss/collision/teardown semantics are tested. Do not add global logging/atomics inside the hot loop merely for this audit.

## HemEnvLerp: implementation and evidence

- R19 is a separate HemEnvLerp DXBC consumer, not the stable shader reused unmodified. Exact paired receivers are rx33/34/35, Lerp pairs 9/10/11. The PTDE consumer filters independent RGBA probes A and B, decodes RGB and alpha separately, applies correct PTDE EnvDiffuse A/B endpoints, then interpolates by beta and removes the DSR-only post-Lerp probe gain.
- `pmetal_rgba_lerp_materializer.cpp` implements `apply_exact_ptde_envdiffuse_lerp_consumer` and sets `envdiffuse_preserved=false`. The companion runtime registration guard under `DSRRL_PMETAL_R19_LERP_EXACT_ENVDIFFUSE` explicitly **requires** this false value; the older inverted registration check was a confirmed failure.
- `pmetal_envspec_draw_runtime::prepare` picks `ptde_envdiffuse_b_required=true` for Lerp, derives canonical probe ordinals from already-authenticated EnvSpec t12/t14, prepares *existing* PTDE EnvDiffuse sidecars, and fails open if either resource cannot be resolved. Stable HemEnv aliases A=B; Lerp needs independently matched A and B.
- R19 has historical owner-side Lerp visual PASS **for that exact P_Metal build only**, NOT for leather. New nonmetal Lerp activation, source A/B pass identity, stable→Lerp transitions and PTDE-visible pixels are **OPEN**.

**Danger:** forcing a stable replacement onto shared rx33/34/35 or skipping the R19 paired registration guard can create a transient stock/ported boundary. Never expand by shader name alone.

## m10 row0: verify provenance, not numerical values

Native RE confirmed alternate FLVER descriptor builder functions `0x140217370` / `0x140217970` can synthesize zero selectorA/B/beta at descriptor +0x4C, reaching a shared return RVA `0x20EB7F`. The current v2.0.3 donor code calls `pmetal_selector_policy::is_synthetic_zero_descriptor(rva, raw_a, raw_b, beta)` to reject `20EB7F/0/0/0`. PR300 nonmetal producer routes through this **same post-materialized code**, so the filter is already inherited and MUST NOT be copied as an independent second patch.

**Residual ambiguity:** `ret_rva == 20EB7F && raw_a == 0 && raw_b == 0 && beta == 0` is not a direct native builder-class proof. A legitimate area0/row0 source from that return could be rejected too. We cannot claim this condition is exact for *all* leather materials without an authenticated builder/provenance/pass discriminator. Preserve genuine row0, do not force m18, and never interpret raw zeros as PTDE brightness/gain mismatch. The proper next criterion is native actual builder identity joined to exact selector owner, receiver and pass, then source-keyed consumer evidence.

On the new nonmetal route, the process-global unkeyed `latest_hook_source` fallback is prohibited, while P_Metal retains historical behavior. Do not "optimize" this safety guard away even if it increases fail-open misses.

## EnvDiffuse sidecar: use the existing P_Metal resource domain

`envspec_resource_runtime.cpp` already keeps `g_ptde_envdiffuse_cubes`, `g_envdiffuse_pack` and `get_ptde_envdiffuse_cube`; this is a per-probe canonical texture resource identity, **not material-owner evidence**. Exact t12/t14 DSR view→canonical probe-ordinal matching is upstream, then `prepare_envdiffuse(probe_a,probe_b,probe_b_required,...)` obtains PTDE RGBA cube SRVs and sampler. Stable aliases A; Lerp obtains B separately. Existing shader consumer uses t11/t13 and exact A/B/beta.

**Potential optimization only after measurement:** lookup/cold GPU cube creation and a mutex may be expensive on first use; warm reuse should not recreate a cube. Do not make second per-material sidecar cache, don't read back/replace DSR t1 alpha or roughness, and don't transfer probe identity from visual similarity or a material name.

## Safe next validation protocol (no gameplay experiment performed here)

1. **Static CI**: verify exact parent / 24-vs-12 material partition, native 5/5 tests, pinned selector donor hash, complete stable+Lerp pair and mandatory `envdiffuse_preserved=false` guard, EnvDiffuse A/B transport. No active renderer source edits in this audit branch.
2. **Unchanged-binary external CPU sampling**: if later authorized, compare authenticated fast and slow restarts of the *same SHA*, same location, camera and gear; record wall-clock frame time, per-thread CPU/RIP and call frequencies. Previous controlled bypasses are closed; do not repeat them.
3. **Optional minimal instrumented *separate diagnostic descendant*** only if external sampling points at addon CPU: low-rate, receiver-gated timing, no per-draw log or unconditional atomic RMW. Report exclusive mean/p95/p99 *us per event* and cumulative *ms per frame* separately for selector source, MTD gate, GPU cache, b12 Map and transaction. Include denominator.
4. **Receiver / semantic verification**: for rx33/34/35 and paired Lerp 9/10/11 independently, prove owner+slot+MTD+current stock gSpec+t10 and probe A/B; quarantine nonmatching views and host states.
5. **Pixel behavior**: controlled PTDE vs vanilla DSR vs same-addon captures for leather and metal; independent normal, roughness, SpecRGB, EnvSpec and EnvDiffuse effects. Shader construction or runtime activation is not pixel PASS.

## Status

**CONSTRUCTION** PR300 PASS; **COMPATIBILITY** Windows CI PASS; **PERFORMANCE ATTRIBUTION** OPEN; **NEW-NONMETAL RUNTIME ACTIVATION** OPEN; **PTDE-PIXEL** OPEN. Audit script/notes are source-only; frozen R43 RC1, PR300 addon binary, PointLight, DoF and protected shader islands unchanged.
