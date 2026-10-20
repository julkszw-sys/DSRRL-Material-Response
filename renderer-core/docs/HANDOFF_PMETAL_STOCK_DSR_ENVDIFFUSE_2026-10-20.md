# Handoff — P_Metal stock DSR EnvDiffuse / HemEnvLerp

**Snapshot date:** 2026-10-20  
**Purpose:** continue the isolated V359 stock-DSR-EnvDiffuse candidate without widening the bridge or weakening fail-open behavior.

## 1. Executive status

- Pull request: [#469 — Add dual-variant V359 candidate with stock P_Metal EnvDiffuse](https://github.com/julkszw-sys/DSRRL-Material-Response/pull/469) — **draft, open**.
- Work branch: `agent/stock-dsr-envdiffuse-20261020`.
- Source commit tested by the latest dedicated CI run: `309ec0907e4d98b6bb11a27304d25b3e5cf34fb7`.
- Pinned parent/base: V359-RC1 source `6e852cdc9085dd175159e564e223e0cc8589c535`.
- Dedicated workflow: [run 39412696183](https://github.com/julkszw-sys/DSRRL-Material-Response/actions/runs/39412696183).
  - Source-contract job: **PASS**.
  - Native Windows single-addon job: **FAIL**.
  - Artifact identity/upload: skipped because compilation failed.
- Generic integrated workflow for the same PR merge candidate also fails earlier at CMake source generation with `FLVER selector profile periodic present log: expected exactly one anchor, found 0`. Treat this as a second compatibility/build issue; do not conflate it with the missing-header compile error below.
- **Construction is not complete.** No candidate addon artifact is available from the latest dedicated run. Runtime activation, bridge activation and PTDE-visible pixel behavior remain **OPEN**. No release or official build has been changed.

## 2. Owner clarification that must be preserved

The intended behavior is **not** “disable Lerp” and **not** “let the bridge apply to all material families.”

1. HemEnvLerp is a first-class path in this candidate. It needs its own stock-EnvDiffuse replacement payload and its own fail-open checks.
2. The candidate is scoped to the exact P_Metal receiver/material route. Materials and aliases outside P_Metal must fail open **for this bridge**, leaving stock DSR behavior. This does not disable their unrelated renderer operators.
3. If receiver/material identity, route decision, source/probe, registered replacement, device/context, CB ABI or draw transaction does not validate, the affected draw must fail open to stock DSR.
4. Do not re-enable the V285/V289/V293 nonmetal alias gates merely to get the candidate compiling. That would reopen the non-P_Metal alias paths this variant intentionally excludes.

## 3. Exact intended semantic cut

Target is PTDE-visible behavior; DSR remains the host renderer. This variant tests one narrow cut: **the P_Metal EnvDiffuse consumer**, not a blanket rollback of Remaster lighting.

### Receiver/material gate

The current exact gate is:

- MTD semantic name: `P_Metal[DSB].mtd`
- Authenticated raw MTD SHA-256: `ece70f36bd2517d28c8495e276cea537f8b519d6bed981788e79a409ffbf763b`
- Route index: `345`
- Receiver IDs: `33–35`
- Exact owner tuple and material slot required.
- Decision must be active, match route 345 and receiver 33–35, and carry finite nonnegative `c101`.

Do not generalize this receiver census to other HemEnv/HemEnvLerp shaders or materials without separate identity and consumer evidence.

### Stable HemEnv

For exact P_Metal only, the stock variant must keep the native DSR EnvDiffuse consumer instructions and native `t11/t13` resource/sampler state. It must not claim EnvDiffuse source/transaction ownership or bind the PTDE EnvDiffuse sidecar at that cut.

The existing PTDE EnvSpec sidecar remains separate at `t12/t14`.

### HemEnvLerp

For exact P_Metal only, the stock variant must retain the original DSR `t11/t13` A/B + beta consumer block byte-for-byte. Keep the stock-mode `b12` payload at **64 bytes**; do not extend it to 96 bytes solely to carry PTDE EnvDiffuse endpoints.

The PTDE EnvSpec A/B + beta path at `t12/t14` remains independently materialized and gated. Lerp must not be treated as unsupported merely because EnvDiffuse is preserved as stock DSR. It must fail open when its own exact receiver/source/probe/replacement/transaction checks fail.

### Cumulative lineage flag distinction

Keep `DSRRL_PMETAL_R19_LERP_EXACT_ENVDIFFUSE=ON` in this build configuration to retain the cumulative R19 feature lineage required by the CMake contract. The stock-mode compile/runtime guards are what suppress the PTDE EnvDiffuse rewrite in this candidate. Do **not** “fix” the candidate by flipping this lineage flag OFF: the current CMake guard explicitly requires it ON when stock mode is enabled.

### Operators intentionally retained

- Existing EnvSpec sidecar and source A/B + beta path.
- MaterialWorkflow, SpecRGB and terminal RGB SAT path.
- Existing independent R3/R5/R11 and other renderer operators.
- DSR host renderer technology outside this exact semantic cut.

The shared R5 normal-basis bridge and existing R3/R11 changes remain in the cumulative lineage. Therefore “stock DSR EnvDiffuse” means stock instructions/resources/samplers at the selected consumer cut; it does **not** prove final pixels equal vanilla DSR. PTDE pixel equivalence remains OPEN.

## 4. Non-P_Metal fail-open configuration

The wrapper `renderer-core/tools/build_v359_stock_dsr_envdiffuse.ps1` deliberately overrides the inherited base-builder flags:

- `DSRRL_V285_ATOMIC_NONMETAL_DIAG=OFF`
- `DSRRL_V289_NONMETAL_LERP_R8_SPEC_LINEAR_AB=OFF`
- `DSRRL_V293_PARSER_EXACT_FLVER_ALIAS=OFF`

The runtime's `exact_pmetal_envspec_candidate` accepts the exact P_Metal gate first and, with V285 disabled, returns false for other material aliases. This is intentional fail-open behavior, not a claim that other materials were reconstructed.

Do not re-enable those aliases as a workaround for the current build error. If non-P_Metal support is pursued later, it requires its own exact owner/FLVER/slot/MTD/resource/consumer proof and a separate candidate, not a silent widening of this bridge.

## 5. Current native-build failure — concrete blocker

Latest dedicated run: [39412696183](https://github.com/julkszw-sys/DSRRL-Material-Response/actions/runs/39412696183), native job ID `118594281513`.

The build successfully compiled `dsrrl_renderer_core.lib`, then failed while compiling the integrated addon. MSVC reported:

```
renderer-core/include/dsrrl/runtime/v315_direct_749_sidecar_ledger.hpp(6,10):
error C1083: Cannot open include file:
'dsrrl/runtime/generated_spec_whitelist_20261010.hpp': No such file or directory

compiling source file:
generated/dsrrl/runtime/integrated_addon_physical_cut.cpp
```

### Source-level cause

- The candidate builder disables V285 to keep non-P_Metal alias routes fail-open.
- The current CMake logic generates `generated_spec_whitelist_20261010.hpp` and adds its generated include directory inside the `if(DSRRL_V285_ATOMIC_NONMETAL_DIAG)` block.
- `v315_direct_749_sidecar_ledger.hpp` includes that generated header unconditionally.
- Therefore the P_Metal-only candidate can compile the core library but fails compiling the integrated addon when V285 is OFF.

### Correct repair direction

Decouple the shared V315 whitelist-header generation/include dependency from the V285 nonmetal diagnostic/alias gate. Keep V285/V289/V293 OFF in this candidate. A minimal safe repair is to ensure the generated header and its include directory are available to any translation unit that includes the V315 ledger, independently of whether the nonmetal alias bridge is enabled. Alternatively, if the ledger truly is optional in this build, guard its inclusion and every use consistently—but do not leave an unconditional include pointing at a conditionally generated file.

Before changing code, inspect all generated headers and include directories currently nested under V285 for similar hidden build dependencies. Add a source/build contract that specifically verifies the stock candidate can build with V285/V289/V293 OFF; string-only flag assertions are insufficient.

## 6. Secondary CI issue

The generic integrated workflow run `39412696189` failed during CMake configure, before compiling the addon:

```
FLVER selector profile periodic present log: expected exactly one anchor, found 0
CMake Error at CMakeLists.txt:566:
Failed to generate Runtime v2 physical-cut integrated source
```

This is distinct from the dedicated candidate's C1083 missing-header failure. Investigate the anchor mismatch against the exact pinned source and generated-source contract. Do not claim general integrated-build compatibility until it passes or the failure is formally shown to be outside this candidate's affected source path.

## 7. Build lineage and release boundaries

- The registered active development base remains V359-RC1, build key `renderer_v359_attested_gi_name_alias_rc1_6e852cdc`, pinned source `6e852cdc9085dd175159e564e223e0cc8589c535`.
- V350-RC1 remains the previous rollback base.
- The frozen V359-RC1 binary must not be overwritten; derive a reproducible descendant.
- Official production release remains unchanged.
- ReShade source is pinned to commit `3645e3025d1d98a90e318278858931f034d5d1f6`.
- The candidate inherits `DSRRL_FULL_TELEMETRY_DEFAULT_ON=ON); it is not a telemetry-free production release and must not be described as one.
- Expected output identity is `out-v359-stock-dsr-envdiffuse/DSRRL_V359_STOCK_DSR_ENVDIFFUSE.addon64`, one integrated addon only.

## 8. Required next steps

1. Fix the V315 generated-header dependency while keeping non-P_Metal gates OFF.
2. Run the source-contract suite and the dedicated Windows single-addon workflow again.
3. Require successful artifact identity checks, exact source-commit marker, stock EnvDiffuse marker, single-addon assertion, and published artifact SHA-256 before calling construction PASS.
4. Triage the generic integrated CMake anchor mismatch separately.
5. Review stable and Lerp fail-open paths explicitly: invalid material/route, receiver mismatch, missing or invalid source/probe, missing replacement, device/context mismatch, invalid CB payload size, and transaction/mutation failure must all preserve stock DSR for that draw.
6. Keep the statuses separate: construction, compatibility, runtime liveness, receiver hit, bridge activation, operator changed, and PTDE-visible pixel improvement. A build PASS is not a runtime or pixel PASS.
7. Do not require new user captures for this construction task. Use permitted existing evidence and code/build validation; leave pixel behavior OPEN unless existing evidence actually supports promotion.
8. Before closing the task, persist the raw CI/source evidence through the DSrrl evidence path, propose/promote only the finding justified by that evidence, verify the new revision via SELECT/bootstrap, run `dsrrl.knowledge_hygiene_audit()`, and re-check the renderer addon control plane. No new Supabase evidence ID has been confirmed for this handoff yet.

## 9. Source files to open first

- `renderer-core/tools/build_v359_stock_dsr_envdiffuse.ps1`
- `renderer-core/tools/test_pmetal_stock_dsr_envdiffuse.py`
- `renderer-core/docs/PMETAL_STOCK_DSR_ENVDIFFUSE_CONTRACT.md`
- `renderer-core/integrated/CMakeLists.txt` — especially V285 whitelist generation and the stock EnvDiffuse guard.
- `renderer-core/include/dsrrl/runtime/v315_direct_749_sidecar_ledger.hpp`
- `renderer-core/src/runtime/pmetal_envspec_draw_runtime.cpp`
- `renderer-core/src/runtime/pmetal_env_source_runtime.cpp`
- `renderer-core/src/operators/env_spec/pmetal_rgba_materializer.cpp`
- `renderer-core/src/operators/env_spec/pmetal_rgba_lerp_materializer.cpp`
- `renderer-core/integrated/integrated_addon.cpp`
- `.github/workflows/v359-stock-dsr-envdiffuse.yml`

## 10. Current confidence boundary

**Confirmed:** the source contract passes at the tested commit; the dedicated native build fails on the missing generated V315 whitelist header; the candidate intentionally disables V285/V289/V293 nonmetal aliases; stable and Lerp are designed as separate stock-EnvDiffuse payload variants.

**Not confirmed:** a successful integrated addon build, runtime receiver hits, bridge activation, the exact runtime frequency of fail-open branches, compatibility of the generic integrated workflow, and PTDE-visible pixel improvement/equivalence.
