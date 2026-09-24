# DSRRL A3 U/L MONOLITH — HANDOFF 2026-09-23

## 0. Canonical working line

Repository:
`julkszw-sys/DSRRL-Material-Response`

**Canonical active dev branch:**
`dev/material-response-expanded-a3-monolith`

Do **not** continue on:
`dev/material-response-expanded-a3-monolithic`
That branch was marked as a legacy duplicate.

Current relevant branch head:
`e8efd13f488ecfc761952208885b5beae3eef47f`

Project target remains PTDE-visible renderer behavior on DSR host.
Current work is specifically the PTDE Upper/Lower runtime bridge inside the Material Response 1.45 host.

Before any further work:
1. `dsrrl.agent_bootstrap(topic)`
2. `dsrrl.renderer_addon_control_plane_status(build_key)`
3. inspect current PROTECT and current branch head.

---

## 1. What the owner observed / why U/L work started

Owner found a useful visual asymmetry around bonfires:

- armor facing a bonfire / local PointLight looks much more plausibly lit and shadowed;
- the side turned away from the bonfire looks too bright / over-filled;
- interpretation: PointLight appears to activate a more plausible material interaction, while the no-PointLight/base path exposes over-bright diffuse/fill.

Paired shader evidence:
- no-PointLight `HemEnv/HemEnvLerp` branch: PTDE delta mask `0x0003`
  - terminal RGB SAT
  - diffuse material-domain linearization
- corresponding `PntS` branch: PTDE delta mask `0x0007`
  - same two operators
  - plus PTDE PntS attenuation linearization

Leading model:
`PTDE-linear material response × DSR Upper/Lower + DSR EnvDiffuse`
is likely a hybrid that can over-brighten the non-direct/base side.

Goal of A3:
first inject **PTDE Upper/Lower** at runtime, leaving EnvDiffuse OFF, so U/L can be isolated before any EnvDiffuse resource bridge.

Verified U/L consumer carrier:
- `b13[6].xyz = Upper_PTDE`
- `b13[7].xyz = Lower_PTDE`

No EnvDiffuse resource replacement is enabled in current A3.

---

## 2. Important architecture decision

Final A3 must be **one `.addon64`**, not 1.45 + A1/A2/U-L companion addons.

U/L must not issue a second draw replay.

Required transaction shape:

`shipping 1.45 PRE`
→ `A3 U/L prepare`
→ **single native DrawIndexed / DrawIndexedInstanced**
→ `A3 U/L restore`
→ `shipping 1.45 POST`

This rule exists because 1.45 already has SpecRGB / asset / material draw transactions. A second replay risks double-drawing geometry and corrupting state.

Current A3 monolith preserves one native draw.

---

## 3. Source preservation / GitHub rule

Owner explicitly required that the Material Response 1.45 source-loss problem must never happen again.

Project-wide PROTECT:
`protect.project.renderer_release_requires_source_complete_git_commit`

A3 is therefore source-first:
- handwritten code committed;
- generators committed;
- exact external input hashes recorded;
- deterministic binary constructors committed;
- build audits committed;
- parent/rollback lineage explicit.

**A3 remains DIAGNOSTIC only.**
Historical full 1.45 source recovery is still incomplete, therefore:
`source_complete = false`
`release_eligible = false`

Do not promote A3 to RC/release while this remains false.

---

## 4. Build lineage and runtime truth

### Build 140 — first U/L monolith
Build key:
`material_response_1_45_a3_ul_monolith_diag_v1`

Binary:
`DSRRL_Material_Response_1.45_A3_UL_MONOLITH_DIAG.addon64`

SHA256:
`75d90c5ed02511f66a37e9265b5be72b94ef4a5e03dbfe939b5fe392700c40f0`

Size:
`2,642,944`

Construction/native:
PASS

Runtime:
**FAIL / crash**

Confirmed crash root cause:
A3 replaced shipping PRE callsite and did:

`legacy_pre()`
→ `a3_capture_pre_ps()`
→ return

but shipping 1.45 expects the **live RAX return from `legacy_pre`** and immediately dereferences it after the callsite (`mov rax,[rax+0x48]`).
A3 capture clobbered RAX.

This is a confirmed ABI regression, not a U/L math failure.

Do not use build140.

---

### Build 141 — RAX Hotfix1
Build key:
`material_response_1_45_a3_ul_monolith_rax_hotfix1`

Binary:
`DSRRL_Material_Response_1.45_A3_UL_MONOLITH_RAX_HOTFIX1.addon64`

SHA256:
`9009b75e28fdd427b7e3329176178f696a2af7ca675138abcd715deb576ca8d0`

Fix:
preserve/restore legacy PRE RAX across A3 capture.

Runtime:
**PASS**
Normal application exit was observed.

But U/L activation:
**FAIL-OPEN**

Log state:
`producer/selector preflight failed; U/L fail-open`

Therefore build141 is currently the **rollback-safe runtime anchor**.

It proves:
- monolithic wrapper can run stably;
- PRE RAX fix is correct;
- U/L had not yet activated.

---

### Why build141 preflight failed

Shipping Material Response 1.45 already owns the EXE selector hook at:

`DarkSoulsRemastered.exe RVA 0x22BA20`

Shipping 1.45:
- installs hook from addon RVA `0x58FD`;
- detour at addon RVA `0x9630`;
- selector resolver at addon RVA `0x9380`;
- resolver callsite at addon RVA `0x9697`.

A3 was trying to install a **second exact-prolog hook** on EXE `0x22BA20` after shipping 1.45 had already modified it.

Correct conclusion:
**do not double-hook selector 0x22BA20.**
A3 must reuse / chain through shipping 1.45 selector transaction.

Canonical finding was written for this.

---

### Build 142 — Selector Chain Hotfix2
Build key:
`material_response_1_45_a3_ul_selector_chain_hotfix2`

Binary SHA256:
`81f0012dc967f2480d1cf42a4f3fec1acb6798fa2ae55864d7e367fd41475ad8`

Mechanism:
- retain Hotfix1 RAX fix;
- do not install second selector hook;
- patch shipping selector resolver callsite;
- run:
  `shipping 1.45 selector resolver`
  → `A3 U/L selector freshness observer`

Important positive result:
Hotfix2 reached:
`PTDE U/L producer armed`

So the double-hook problem was real and was solved.

Runtime:
**FAIL / owner reports game crashes**

Build142 must NOT be reused as a working baseline.

No definitive crash stack was supplied for this crash.
Do not label exact Hotfix2 crash instruction CONFIRMED.

---

## 5. Leading Hotfix2 crash mechanism

Status:
**HIGH CONFIDENCE candidate, not confirmed by crash stack**

When Hotfix2 finally activated A3 selector observation, newly-live A3 code read selector descriptor state.

Historical validated U/L runtime used guarded reads.
The A3 selector path had a risky selector tuple read path.

Current tuple:
- chosen descriptor comes from r14 or r15 depending on selector return site;
- tuple lives at `desc + 0x4C`;
- exact tuple size = 8 bytes:
  - `u16 A`
  - `u16 B`
  - `u32 beta`

Known selector return RVAs:
- `0x20E019`
- `0x20EB7F`
- `0x20FB9E`

Shipping 1.45 already contains a verified guarded 8-byte reader:
addon RVA `0x2A60`.

This led to Hotfix3.

---

## 6. Build 143 — Safe Observer Hotfix3 CURRENT CANDIDATE

Build key:
`material_response_1_45_a3_ul_safe_observer_hotfix3`

Version:
`1.45-A3-UL-SAFEOBS-HF3`

Binary:
`DSRRL_Material_Response_1.45_A3_UL_MONOLITH_SAFE_OBSERVER_HOTFIX3.addon64`

SHA256:
`052dc7d277d67a006e6df57c5da86d94438858479056a7418190d9bff8b07b0b`

Size:
`2,642,944`

PE checksum:
`0x00285C35`

Status:
- CONSTRUCTION: PASS
- NATIVE: PASS
- RUNTIME: **NOT_TESTED**
- PIXEL: OPEN
- RELEASE ELIGIBLE: NO

Rollback build:
141

Hotfix3 behavior:
- Hotfix1 RAX preservation retained.
- Hotfix2 single-selector ownership retained.
- shipping 1.45 selector resolver still runs first.
- before A3 observer executes, a wrapper validates exactly 8 bytes at selected `desc+0x4C`;
- validation uses shipping 1.45 safe-read helper RVA `0x2A60`;
- invalid pointer / unknown selector return site => fail-open, A3 observer is not entered;
- four LightBank producer hooks unchanged;
- draw callsites unchanged;
- U/L payloads unchanged;
- EnvDiffuse OFF.

Binary patch details:
- Hotfix2 chain call RVA: `0x1C1D8B`
- Safe wrapper RVA: `0x1C1E00`
- shipping EXE-base global addon RVA: `0x1076B8`
- shipping safe-read8 addon RVA: `0x2A60`
- existing A3 observer RVA in build142/143: `0x1BFC50`

Relevant GitHub commits:
- `a4f39328d5803e7b05da780613d5a0162a387c7a`
  - guard selector tuple with shipping 1.45 safe-read helper
- `8bbb8d4738c6206e27e67e6de2de678d31175656`
  - guarded selector observer wrapper ASM
- `e8efd13f488ecfc761952208885b5beae3eef47f`
  - reproducible Hotfix3 binary constructor

Relevant source:
- `experimental/expanded-a3/src/a3_ul_core.cpp`
- `experimental/expanded-a3/src/a3_safe_observer_wrapper.s`
- `experimental/expanded-a3/tools/hotfix_safe_observer3.py`

---

## 7. Existing source-level U/L bridge

Current source has no-replay U/L transaction code.

Important code:
`experimental/expanded-a3/src/ul_integrated_bridge.cpp`
`experimental/expanded-a3/src/ul_integrated_bridge.hpp`
`experimental/expanded-a3/src/selector_hook.asm`

The older validated U/L source was also preserved as provenance on the earlier development lineage.
Do not rewrite producer equations from memory.

Producer semantics:
- capture LightBank endpoint A/B + beta before DSR destroys the PTDE source representation;
- decode PTDE RGBM endpoints linearly;
- linear blend endpoints;
- publish Upper/Lower snapshot;
- selector-side owner + A/B + beta freshness must match before draw.

Do not reconstruct U/L from already-processed DSR `c98/c99`.
The point of the bridge is to preserve PTDE source-domain semantics.

---

## 8. Receiver routing / U/L consumer scope

Confirmed no-PointLight HemEnv receiver registry:
receiver indices 24..47.

Representative mapping:
- receiver33 → shader 894 `FRPG_Phn_DifSpcBmp______Csd_HemEnv.fpo`
- receiver34 → shader 913 `FRPG_Phn_DifSpcBmp______Sdw_HemEnv.fpo`
- receiver35 → shader 932 `FRPG_Phn_DifSpcBmp__________HemEnv.fpo`

P_Metal protection remains active:

`protect.renderer.pmetal.no_shared_body_global_patch_without_exact_material_gate`

Do not identify P_Metal from shared shader identity alone.

P_Metal route:
`route_index 345`

P_Metal U/L payloads were generated from the **shipping 1.45 P_Metal alternatives**, not vanilla/P2.2, so U/L must not roll back current P_Metal/SpecRGB/material-response behavior.

Generic U/L consumers were generated from the certified UL48 host census.

---

## 9. A1 / A2 status — important clarification

Earlier work:
- A1 Expanded Material Response:
  - build138
  - runtime PASS
  - expanded closed operator plans
- A2 Receiver Census:
  - build139
  - runtime PASS
  - read-only receiver census
  - showed real active PntS shared-specular bodies

However the current A3 binaries 140–143 were built as:
**shipping 1.45 + A3 U/L extension**

They are **not yet a verified physical merge of the complete A1/A2 executable logic into the same PE**.

Do not claim “full 1.45 + A1 + A2 + U/L monolith” yet.

A2 telemetry is diagnostic anyway.
A1 closed Material Response still needs deliberate integration into the final one-addon source line after U/L liveness is stabilized.

---

## 10. What NOT to do next

Do not:
- reuse build140;
- reuse build142;
- reintroduce a second hook at EXE `0x22BA20`;
- reintroduce a second DrawIndexed replay;
- remove RAX preservation from PRE wrapper;
- bypass exact material routing for shared P_Metal-capable bodies;
- enable EnvDiffuse while U/L runtime liveness is unresolved;
- enable PTDE EnvSpec cubemap replacement;
- call construction PASS a pixel PASS;
- call Hotfix3 runtime PASS before it is actually run;
- promote A3 to RC/release while `source_complete=false`.

---

## 11. Exact next step for the next agent

Start with:

`dsrrl.agent_bootstrap("A3 Hotfix3 safe selector observer runtime / liveness")`

then:

`dsrrl.renderer_addon_control_plane_status("material_response_1_45_a3_ul_safe_observer_hotfix3")`

Then inspect:
- canonical branch head;
- build143 metadata;
- `a3_ul_core.cpp`;
- `a3_safe_observer_wrapper.s`;
- `hotfix_safe_observer3.py`.

Build143 is the current diagnostic candidate.

If runtime evidence for build143 becomes available, classify separately:
1. addon loaded;
2. producer armed;
3. selector freshness matched;
4. receiver/route matched;
5. `FIRST PTDE U/L DRAW ACTIVE`;
6. restore PASS;
7. normal exit;
8. pixel behavior.

Do not promote one level from another.

If build143 still crashes:
- do not touch draw code first;
- inspect safe wrapper ABI / legacy selector resolver call contract / preserved volatile and nonvolatile registers;
- compare exact Hotfix3 callsite against original shipping selector detour;
- fail-open back to build141 if uncertain.

If build143 is stable but no `FIRST PTDE U/L DRAW ACTIVE`:
then move to routing/activation:
- selector match count,
- receiver index,
- pre-PS/current-PS relation,
- route/material gate,
- U/L shader cache/materialization,
- b13 realization/bind.
Do not change U/L equations until routing is proven.

---

## 12. Current safest rollback

If any new A3 candidate is unstable:

**rollback to build141**

SHA256:
`9009b75e28fdd427b7e3329176178f696a2af7ca675138abcd715deb576ca8d0`

Known state:
- runtime stable / normal exit;
- U/L fail-open;
- no PTDE U/L activation;
- useful as stable monolithic host for further static integration work.

Shipping 1.45 remains the ultimate clean production rollback:
SHA256:
`e44183ef10fc7921f7741eb16d54ec30e814f580421ac6c83be18b5308b73342`

---

## 13. Supabase state

Current control-plane build:
`material_response_1_45_a3_ul_safe_observer_hotfix3`
build id: **143**

Build142:
runtime **FAIL**

Build141:
runtime **PASS**
bridge activation **FAIL_OPEN**

Build143:
construction/native **PASS**
runtime **NOT_TESTED**
pixel **OPEN**

Current control-plane revision observed during handoff:
**8791**

Knowledge hygiene audit at handoff:
only the two longstanding unrelated WARNs remain:
- `77fbd368-5e89-4dd5-8525-81bc40469190`
- `5086580d-578c-4148-9025-b2d94a24fe90`
both `SOURCE_SHA_SCOPE_UNDECLARED`, artifact 22.

---

## 14. Final status snapshot

CONSTRUCTION:
PASS for build143.

COMPATIBILITY / NATIVE:
PASS for build143 construction.

RUNTIME LIVENESS:
- build140 FAIL
- build141 PASS
- build142 FAIL
- build143 NOT_TESTED

BRIDGE ACTIVATION:
- build141 FAIL-OPEN
- build142 producer ARMED before crash
- build143 OPEN

PIXEL BEHAVIOR:
OPEN.

EnvDiffuse:
OFF.

PTDE EnvSpec replacement:
OFF beyond existing shipping policy.

Next target:
**stabilize selector freshness / U/L activation first. Do not expand operator scope until that is done.**

---

## 15. HF3 runtime result and HF4 source/runtime repair

Owner runtime log for build143 / HF3 is **FAIL**. ReShade 6.8.0.1 and the addon load/register correctly, the A3 producer arms, swapchain creation succeeds, then the process terminates immediately after the first ResizeBuffers/runtime recreation boundary. This matches build142 and differs from build141, which survives the same boundary.

Confirmed source/runtime mismatch in HF3:
- current source `a3_selector_observer` uses `a3_safe_selector_tuple()`;
- the actual HF3 binary still contains raw tuple loads at addon RVA `0x1BFD7F`: `[rdi+0x4C]`, `[rdi+0x4E]`, `[rdi+0x50]`;
- HF3 only added an outer wrapper precheck, then entered the old raw-reading observer body.

HF4 construction therefore patches the observer itself:
- base HF3 SHA256: `052dc7d277d67a006e6df57c5da86d94438858479056a7418190d9bff8b07b0b`
- output SHA256: `a17031743c48dfa27c8dabd07673320ba4b22b61c3d55287ad0f436db406327b`
- observer redirect: RVA `0x1BFD7F` -> executable cave RVA `0x1C1F00`
- trampoline calls shipping safe-read8 RVA `0x2A60`
- success resumes at `0x1BFD8B`
- failed read branches to existing fail-open RVA `0x1BFE18`
- PE checksum: `0x0028D10C`

Reproducer:
`experimental/expanded-a3/tools/hotfix_safe_tuple_observer4.py`
(commit `235a2acfc1b36c66ddfe68c988a3ffacd7f7900c`)

Audit:
`experimental/expanded-a3/audit/SAFE_TUPLE_HOTFIX4.json`

HF4 status:
- CONSTRUCTION: PASS
- RUNTIME LIVENESS: NOT_TESTED
- BRIDGE ACTIVATION: OPEN
- PIXEL BEHAVIOR: OPEN
- EnvDiffuse: OFF

Safest confirmed runtime rollback remains build141 / HF1. HF4 is the next diagnostic construction, not a promoted runtime PASS.


---

## 16. HF4 runtime FAIL and HF5 producer-contract correction

Owner runtime log confirms build144 / HF4 is also **RUNTIME FAIL**. It loads/registers, arms the A3 U/L producer, creates the swapchain, then terminates at the same first-ResizeBuffers/runtime-recreation startup boundary as HF2/HF3. Therefore selector tuple safe-read placement was not sufficient to restore liveness.

A deeper lineage audit changed the interpretation of build141 vs build142:
- build141's selector preflight failure runs `restore_hooks()`, so its runtime PASS is a fail-open control where the newly installed A3 producer hooks are removed;
- build142 skips the conflicting selector install and therefore is the first monolithic successor that leaves A3 producer hooks live;
- the common HF2-HF4 crash interval therefore does **not** isolate the selector chain by itself.

More importantly, builds142-144 were still using a producer model superseded by canonical finding
`project.branch.renderer_edition_ul_real_producer_capture_contract_v1` rev4401:
- EXE `0x140564510` + returns `0x140563642/659` are cache-builder calls, not ordinary steady U/L;
- ordinary steady U/L is selected by `0x140563460` from cached 0x110-byte records and should be recovered through the already-routed single packer `0x140563B80`;
- true interior blend capture at `0x1405642F0` remains valid.

### Build145 — Producer Contract Hotfix5

Build key:
`material_response_1_45_a3_ul_producer_contract_hotfix5`

SHA256:
`6fde1cf09a283ec1bd5a966690dc3407b48c757257d5d79c3fba07a948293f34`

Construction:
PASS, exact deterministic reproduction PASS.

HF5:
- removes installation of the obsolete `0x140564510` A3 SINGLE hook;
- restores guarded 8-byte post-wrapper assignment A/B/beta read through shipping 1.45 safe-read8;
- retains wrapper5/wrapper6 capture;
- retains true interior blend capture `0x1405642F0`;
- retains HF4 selector safety and shipping selector resolver;
- leaves steady U/L fail-open until canonical `0x140563B80` integration is materialized;
- keeps EnvDiffuse OFF.

Source alignment commit:
`f4ac43a385e5d12efc4c7797302c7138f8eece21`

Patch generator commit:
`e39f5e30d972e9fe1b9e9ea648be7557cb1849a4`

Audit commit:
`0591b208a0736aa6cbc6d58689c335c9d40df922`

Status:
- CONSTRUCTION: PASS
- NATIVE: PASS
- RUNTIME: NOT_TESTED
- BRIDGE ACTIVATION: PARTIAL_BY_CONSTRUCTION
- PIXEL: OPEN

Do not promote HF5 to runtime PASS from construction alone. Build141 remains the last confirmed runtime-live fail-open rollback.


---

## 17. HF5 runtime result — later loading-screen failure

Owner runtime on build145 / HF5:
**FAIL — crash on loading screen.**

This is not the same observed boundary as HF2-HF4.

Log chronology:
- addon loads and registers;
- A3 U/L producer arms;
- first fullscreen/ResizeBuffers cycle completes;
- ReShade runtime is destroyed and recreated successfully;
- process continues for approximately 11.8 seconds;
- loading phase creates a burst of deferred D3D11 contexts;
- log then terminates.

No `FIRST PTDE U/L DRAW ACTIVE` line appears before termination.

Interpretation:
- HF5 did not achieve runtime liveness;
- the earlier immediate post-ResizeBuffers failure boundary is no longer the observed failure point;
- removal of the obsolete `0x140564510` hook / producer guard correction therefore changed runtime behavior materially;
- exact crash instruction remains OPEN;
- the final `CreateDeferredContext` lines are temporal markers only, not proof that context creation itself is causal.

Current residual crash surface is after A3 is armed and before a confirmed successful PTDE U/L draw transaction. Candidate domains to isolate next are:
1. wrapper/blend producer execution under real area loading,
2. selector freshness chain under live LightBank traffic,
3. receiver/pre-draw capture and first materialization,
4. D3D11 immediate-vs-deferred-context assumptions in shader/CB13 state handling.

Build145 remains DIAGNOSTIC. Rollback remains build141.
