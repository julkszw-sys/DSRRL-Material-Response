# V301 HANDOFF: 1295 October SpecRGB full atomic target (2026-10-16)

**Read this before modifying Renderer Edition.** Full self-contained Polish handoff was delivered in conversation ZIP `DSRRL_FULL_ATOMIC_SIDECAR_HANDOFF_V301_2026-10-16.zip` with full 1295 CSV and MTD family matrix. Original static matrix: `renderer-core/data/census/v300_oct10_1295_atomic_coverage_source_contract.md`.

## Strict target
PTDE-visible operator behavior on DSR host. All 1295 Oct10 logical SpecRGB names must have verifiable exact owner/receiver/operator-local producer/resource/material/CB/shader routing, with complete atomic request or operator-local stock-DSR fail-open. No per-name alias, no blanket PBL off, no partial PTDE diffuse/DSR spec mix. R43 P_Metal is the transaction architectural reference, not universal shader.

## Source findings and status
- PTDE full FLVER: `DSRRL_PTDE_FLVER_CENSUS_FIXED.zip` Library `/DSRRL/source-corpora`, SHA256 `6651acb4fe995a69f1c84e5761b3d6bdcadeb39e1fc4d1087c3b82794f4d7281`.
- Original Oct10 names: 1295. 1293 source PTDE `g_Specular`, 749 direct DSR `g_Specular`, 544 PTDE `*_L_s` with actual separate PTDE FLVER source, 2 names `wp_a_0911_s`, `wp_a_1004_s` without direct PTDE/DSR FLVER g_Specular owner. 544 LOD names are **NOT** safely aliases of DSR base.
- V299 slot0 `P_DullLeather`/Edge/`Ps_Body`: 1987 source rows / 407 names. V300 slot1 `P_Leather`/Edge: 793 source rows / 248 names. Union of new candidates 552 names (overlap). None of these figures is runtime/pixel PASS.
- Current draft branch `agent/v294-all-sidecar-manifest-atomic-20261015`, PR #400. Pin before making new modifications; as of this handoff source audited at `584caaf7148b6c8daa897e35f8e4a30169a67fea`.
- Existing baseline R43 RC1 has separate build/liveness and known pixel FAIL; V301 binary native build/runtime/pixel were **not** confirmed in this handoff.

## EnvDiffuse is in the source-connected atomic operator
Two distinct PTDE sidecars:
1. `DSRRL/EnvSpec/PackedGI/PMETAL_PTDE_ENVDIFFUSE_DONOR_V1.bin` (15,296 bytes, 1,246 PTDE rows, 20 banks). `pmetal_env_source_runtime.cpp` loads exact SHA and decodes A/B LightBank EnvDiffuse endpoints.
2. `DSRRL/EnvSpec/PackedGI/PTDE_GI_ENVDIFFUSE_PACK_RGBA.bin` (8,404,992 bytes), exact SHA and PTDE GI probe materialization in `envspec_resource_runtime.cpp`, bind `t11`/`t13` as appropriate.
Separate EnvSpec PackedGI `PTDE_GI_ENVSPEC_PACK_RGBA.bin` (33,619,968 bytes) with `t12`/`t14`; SpecRGB uses exact `t10`. `pmetal_envspec_draw_runtime.cpp` composes shader + b12 + resources/samplers in one draw transaction, with material/receiver gates. This is **source implementation**, not global runtime/pixel PASS.

## Critical donor policy (user explicit)
`renderer-core/data/provenance/v300_lightbank_donors_ptde_dsr_only.tsv`: 1,249 rows = 1,246 homologous original PTDE + **3 original DSR-only rows**. For PTDE homologues use exact PTDE EnvSpec and independent PTDE EnvDiffuse endpoints, no guessed gains. For DSR-only **preserve exact original DSR values for BOTH operators**, currently by fail-opening entire operator to native DSR; never borrow row63/stale PTDE/global latest hook.
- `a14/m14` row64 DSR EnvDiffuse `255,255,255,1500`; EnvSpec `255,255,255,200`.
- `a14/s14` row64 EnvDiffuse `255,255,255,500`; EnvSpec `255,255,255,200`.
- `a18/m18` row64 EnvDiffuse `255,255,255,150`; EnvSpec `255,255,255,150`.
Exact bank signature, DSR layout, live count, donor count and row ID mandatory. Unrecognized rows fail-open. `materialize_v300_donor_provenance.py` stops unkeyed `latest_hook_source` stale PTDE fallback under `DSRRL_V300_DSR_ONLY_EXACT_DONORS`. **Direct mixed-shader DSR values are not yet proven**; native DSR full operator is correct conservative fall-through.

## Immediate next actions
1. Supabase `dsrrl.agent_bootstrap` and `dsrrl.renderer_addon_control_plane_status`, inspect PROTECT with scope; check GitHub latest HEAD.
2. Build ONE V301 `.addon64` using `renderer-core/tools/build_v301_mixed_donors_one_addon.ps1` / `.github/workflows/v301-mixed-donor-atomic.yml`; repair real CI and compilation failures. Source tests are not native compilation/pass.
3. Verify exact sidecar files staged and runtime loaded, producer and receiver hit, full b12 and t1/t10/t11/t12/t13/t14 bindings and state restoration. Report fail-open reasons.
4. Test PTDE row63 -> DSR-only row64 -> PTDE row63 for all three banks, A/B blended, no leaked PTDE source; exact DSR-only native EnvSpec/EnvDiffuse.
5. Complete nonhomologous LOD/asset island for 544 `*_L_s` and weapon resource identity for two exceptions. Extend correct PTDE shader family operators across rest of MTDs; do not assume P_Metal operator globally.
6. Keep per-name source/compatibility/runtime/activation/pixel states distinct and compare PTDE-visible pixels across maps, shaders, receiver/MTD types. Full 1295 atomic cannot be claimed yet.

Always save evidence -> proposed/promoted finding -> revision/hygiene/control. No changes on disk to `DarkSoulsRemastered.exe`.
