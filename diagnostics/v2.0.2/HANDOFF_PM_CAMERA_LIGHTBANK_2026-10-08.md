# DSRRL v2.0.2 — FULL NEXT-AGENT HANDOFF
## P_Metal whole-surface light variation during camera-only rotation
Date: 2026-10-08. Status: **new diagnostic construction PASS; root cause/pixel OPEN**.

## 0. Objective / rules
PTDE visible behavior is target; DSR is host. One reproducible ReShade .addon64 plus exact sidecars. No on-disk DarkSoulsRemastered.exe modification. No guessed lighting, bloom, gamma, ambient or exposure gains. Preserve independent stock DSR operators and fail-open unless a precise PTDE operator semantic cut is proven.

MANDATORY START before any task:
1. Supabase SQL: select dsrrl.agent_bootstrap('PMetal')
2. Supabase SQL: select dsrrl.renderer_addon_control_plane_status(null)
3. Inspect canonical findings/revisions, REJECTED, PROTECT rules with their scope, renderer operators, mappings and current CI/release lineage.
MANDATORY FINISH: evidence -> finding -> promotion only if verified; check revision, run dsrrl.knowledge_hygiene_audit(), recheck control plane. Keep separate construction / compatibility / runtime / bridge activation / pixel statuses.

Owner observation: **character stands completely motionless at one unnamed location; rotating only the camera changes whole-surface brightness of all armor types**. Not merely a moving reflection. Not a RoughCloth-only problem.

## 1. Exact release and development pointers
Repository: https://github.com/julkszw-sys/DSRRL-Material-Response
Production public v2.0.2: https://github.com/julkszw-sys/DSRRL-Material-Response/releases/tag/v2.0.2
Production source: b19a273f15872999a631e3feaa0a0dd6aea35e89, branch release/v2.0.2-sync-on-4way.
Production addon SHA256: ef727a1d870500eec58fcb9ed55a29da196512fc965d10fb70dd229ad51e8a52. KEEP UNCHANGED.
PR #279 all-material MR diagnostic: https://github.com/julkszw-sys/DSRRL-Material-Response/pull/279
PR #280 targeted source origin tracer: https://github.com/julkszw-sys/DSRRL-Material-Response/pull/280
PR280 head branch: diagnostics/v2.0.2-pmetal-spec-transition-01; base: diagnostics/v2.0.2-full-mr-cloth-trace-01 (historical misleading name; actual base logs all MR materials).
**Verified new diagnostic code commit:** f080738eaf634f7897caa3a928715ac392d06024.
**Latest Windows CI PASS:** https://github.com/julkszw-sys/DSRRL-Material-Response/actions/runs/37834019496 (3 native groups PASS, single-addon ZIP integrity PASS).
**Owner-download artifact:** https://github.com/julkszw-sys/DSRRL-Material-Response/actions/runs/37834019496/artifacts/11574817739
Artifact name: DSRRL_v2.0.2_PMETAL_SPEC_TRANSITIONS.
Expected addon: DSRRL_v2.0.2_PMETAL_SPEC_TRANSITIONS.addon64.
**SHA256 of addon binary:** 483649ebf1aed4b813087ff69c064083f686842cf072aeff130c046bf68d4d1d. Do not confuse with outer Actions archive digest.
CI compile/flags: R43; KEY_BUCKET_256 SYNC ON; companion TLS 4WAY; full diagnostics and native D3D state verification; unchanged MR/PMetal/PointLight/UL/H3/Subsurface operator gates. No owner game runtime on this NEW f080 build, no matched PTDE pixel test.

## 2. Runtime evidence with dates and exact distinctions
User-uploaded logs are private conversation files and should not be republished unredacted to public GitHub.

A) ReShade(20261008-190504).log, previous all-material diagnostic:
- MR and P_Metal genuinely active; rx34 route345 shows stock SpecRGB t1 bound but unresolved logical snapshot/hash and no PTDE companion (snapshot=0, companion=0). Spec gate fail-open at ~21:04:26. Aggregate 278 spec rejections / 19539 P_Metal candidates.
- VALUE CUT first-logs probe340 at rx33 and probe337 at rx34, same bank row32/beta0. These are **different receivers, not evidence of one receiver switching probe during camera rotation**.
- Material census exhausts 512 lines before failure. Do NOT infer causal time correlation.

B) ReShade(20261008-193204).log, targeted diagnostic commit d5fa4b049186b0020f9c7ac2ff5e3ea1514d69ae:
- 1024 PMETAL SPEC TRANSITION events over approximately 21:31:26.938–21:31:31.383: **1023 PTDE_PREPARED**, only **1 STOCK_FAILOPEN**.
- Only sampled STOCK_FAILOPEN belongs to different FLVER prefix **3a82e4f6**, slot2, rx34, probe341, row35, live t1 but registry=0/snap=0/companion=0. NOT the repeatedly observed material below.
- Aggregated by 21:31:44: spec_reject=93 / candidate=8373, req=8280, source_reject=0, probe_reject=0. Later aggregate cannot be mapped to an owner because detailed trace had reached cap.

**Critical same-material observation (B):**
- FLVER SHA256 prefix **672e3560**, material slot **1**, receiver **33**, route **345**, HemEnv family0.
- Same SpecRGB t1 pointer **000000005f3e4ba8**, logical hash **881b6746c5597a31**, registry=1, snap=1, allowed=1, companion=1; not quarantined.
- Same EnvSpec probe A/B **340/340**, t12/t14 **0000000049ca83a8**, beta=0, c101=2.5, same bank ID **1ecfd1e617c59071**.
- While actor position fixed and only camera rotated, LightBank rowA/B changed **32 -> 22 -> 23 -> 22 -> 23 -> 22**. source_serial also changed. The same sampled material remained **PTDE_PREPARED** in all 221 captured samples.
- Anchors: ~21:31:26.940 row32; ~21:31:29.067 row22; ~21:31:30.500 row23; ~21:31:30.600 row22.
- FLVER prefix is logged (4-byte digest prefix), not full identity; actual exact routing uses full hashes. Also log has no per-frame camera matrix or paired PTDE pixels.

**VERIFIED:** user stationary/camera-only report; same sampled material+receiver has stable SpecRGB/probe but variable LightBank row; code provides a last-source fallback; distinct material sometimes fails SpecRGB. **NOT VERIFIED:** exact native source origin of changing row, pixel-level cause, temporal correlation to visible brightness, or whether PTDE source itself should be camera-invariant in this receiver family.

## 3. Code routing vulnerability to isolate
- renderer-core/src/runtime/pmetal_env_source_runtime.cpp
  pmetal_env_source_runtime::latest(material,out) first calls pmetal_producer_state_latest(material,epoch,out). On failure it calls latest_hook_source(out).
  latest_hook_source returns **last EnvSpec packer TLS source**, or last **global** packer source; it has no material-slot key check in that fallback. This is a concrete code-level possibility of cross-material source contamination when camera rotation changes draw ordering. **HYPOTHESIS, NOT CONFIRMED AS ACTIVE PATH**.
  publish_hook_source() and native selector hooks produce LightBank source banks/rows/serials.
- renderer-core/src/runtime/pmetal_producer_state.cpp
  Exact per-material producer state uses full FLVER SHA, raw MTD SHA, material slot, semantic/family, epoch; fast TLS plus synchronized material-key path. pmetal_producer_state_begin invalidates exact record before attempting source resolution.
- renderer-core/include/dsrrl/runtime/pmetal_selector_policy.hpp
  Upper selector bits = area; low byte = row, with area/type fallback. Source may change for reasons not obvious from actor motion. Need compare PTDE exact operator and native selector for proof.
- renderer-core/src/runtime/pmetal_envspec_draw_runtime.cpp
  P_Metal prepares source, companion, probe and shader transaction. Missing exact SpecRGB companion => stock DSR fail-open on that draw (correct policy).
- renderer-core/src/runtime/material_resource_draw_runtime.cpp
  Exact stock t1 SRV registry/logical identity, sidecar companion. Do not weaken this gate to fake a fix.

Candidate mismatch pipeline:
native LightBank/selector producer -> exact FLVER/slot source cache or unqualified latest packer fallback -> P_Metal CB/shader source -> whole-surface visible output. Identify producer provenance before patching.

## 4. Old tracer failure / current f080 diagnostic changes
Old d5fa tracer used 128 TLS slots indexed (owner_key xor owner_key>>32) & 127. The material slot was added by shift-left 16 and therefore had **no entropy in the low seven index bits**. Different material slots of the same FLVER+receiver collided, forcing repeated logging. The hard cap of 1024 lines exhausted in ~4.4 s, before later aggregate failures; its transition predicate omitted row/origin.
New f080 source:
- Adds read-only pmetal_envspec_source.diagnostic_origin behind DSRRL_PMETAL_SPEC_CUT_TRACE:
  **source_origin=1** exact per-material selector producer.
  **source_origin=2** most recent EnvSpec packer on same thread (unqualified).
  **source_origin=3** most recent EnvSpec packer globally (unqualified).
- Fixes slot/receiver/family key mixing via FNV; increases cache to 512 TLS entries.
- Logs LightBank row A/B changes and source_origin changes immediately; otherwise 500 ms samples; total hard cap 4096 records.
- Includes srcA RGB, envdiffuseA RGB, bank/row, serial, probeA/B, t12/t14, t1/companion and prepare status.
- Does NOT modify material shader, light values, SpecRGB authorization, resource bridge behavior, native shader ABI or sidecars.
- CI 37834019496 PASS; new runtime on owner machine OPEN.

## 5. Next operator-isolation test
1. Owner installs **artifact 11574817739** (NOT older 11573542877), one addon at a time, PTDE sidecars intact.
2. At exact problematic spot, character stays still. Rotate ONLY camera for at least 10–20 s, then stop; repeat with another armor.
3. Return full ReShade log. Correlate camera movement time to **same exact** owner/slot/rx in [DSRRL PMETAL SPEC TRANSITION].
4. Compare source_origin, rowA/B, srcA, envdiffuseA, source_serial, t1 hash and probe in consecutive records.
5. **origin 2 or 3 + changing row:** suspect unqualified latest packer; prove producer/consumer ordering, then route exact source to exact material cut (do not hardcode row).
6. **origin 1 + changing row:** examine native exact selector a/b, pointer, area/type, producer output; determine whether PTDE and DSR selectors ought to change with camera. No patch based on actor position alone.
7. **no row change but visible brightness shift:** examine reflection vector / normal basis / material response / downstream composition and actual draw transaction, not source row.
8. **same-material PTDE_PREPARED to STOCK_FAILOPEN:** only then pursue t1 SRV registry/epoch/companion lifecycle for that exact slot; do not use other receiver failure as proof.
9. If ambiguous, add a further narrow trace of full FLVER/MTD hash, native selector and source pointer, producer/consumer thread and epoch, camera/view CB and destination RT. Avoid multi-thousand-line log flood.
10. Compare matched PTDE-visible pixels and cross-check multiple armor material families before calling pixel behavior PASS.

## 6. No-shortcut guardrails
- No LightBank +20%, global gain, gamma, exposure, tone-map, bloom, ambient, PBL/TAA/SSS rollback, blanket EnvSpec-off, or estimated probe remap.
- Do not force PTDE SpecRGB when companion unauthorized; preserve fail-open on unknown receiver/material/asset.
- Keep material diffuse, normals, SpecRGB and reflection/EnvSpec as distinct operators; do not assume a broad P_Metal observation proves MR diffuse equation wrong.
- Do not edit DarkSoulsRemastered.exe on disk.
- A screenshot may falsify and localize, but cannot solve source mapping.
- Separate build/runtime/activation/pixel evidence; do not claim fix from successful CI.

## 7. Supabase and pending actions
Supabase canonical revision **12023**: renderer.pmetal.stationary_actor_lightbank_row_transition_20261008_v1, confirms same-slot row variation, leaves exact source provenance and PTDE pixels OPEN.
Previous relevant revisions: 12014 (rx34 SpecRGB fail-open), 12017 (user whole-surface camera observation), 12020 (older targeted CI diagnostic).
Record f080 CI success as construction-only evidence if not already promoted. Next runtime evidence must name exact owner slot, receiver, row, source_origin, t1/probe and camera moment. Do not promote latest hook fallback as proven root until observed. Run hygiene audit and control-plane checks before final.

## 8. Takeover in one paragraph
At a stationary player location camera rotation visibly changes the brightness of all armor types. Log B shows one repeatedly observed P_Metal material stays PTDE_PREPARED with stable t1 SpecRGB and probe340, but LightBank row changes 32/22/23. The alternate stock-fallback sample belongs to a different material. Source code contains an unqualified latest EnvSpec packer fallback if the exact per-material selector producer is unavailable. f080 diagnostic addon (CI PASS) now identifies exact source vs TLS/global fallback, logs RGB/row transitions, and fixes the old tracer collision. **Next step: run f080 on user PC, inspect source_origin and the row RGB values for same FLVER+slot+receiver. Root cause and PTDE pixel improvement remain OPEN.**
