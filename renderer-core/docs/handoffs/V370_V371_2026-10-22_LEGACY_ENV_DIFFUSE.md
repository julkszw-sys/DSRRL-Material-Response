# DSRRL V370 → V371 exact handoff · 2026-10-22

**Status:** handoff, not PTDE pixel PASS. This file is a permanent repository index. Full portable handoff is in owner-provided archive `DSRRL_V370_TO_V371_HANDOFF_KOMPLETNY_2026-10-22.zip` (ZIP SHA256 `6be3dffb7c9ed53112b641a0e98e59cbe7239c8a46003f5f52677a9d101d3d19`, 6,001,449 bytes). The archive includes full 15-section `HANDOFF_MAIN_PL.md`, sidecars, source verifier, manifest, owner logs and V370 binary. The next chat MUST receive that ZIP rather than reconstruct missing inputs.

## Task and source of truth

User target: **isolate original complete PTDE EnvDiffuse from EnvSpec and integrate legacy EnvDif sidecar**, retaining PackedGI 342, native DSR EnvSpec/other independent systems, strict fail-open, one reproducible ReShade `.addon64`, unchanged on-disk DSR EXE. No generic light/PARAM/exposure compensation.

Start with `dsrrl.agent_bootstrap(topic)` and `dsrrl.renderer_addon_control_plane_status(null)`; inspect canonical, REJECTED, scope PROTECT, and known gaps. End with evidence → finding → promote if proven, verify revision, `knowledge_hygiene_audit()`, control plane. Separate construction/Win64/runtime/activation/pixels.

**V370** draft [PR510](https://github.com/julkszw-sys/DSRRL-Material-Response/pull/510), **source HEAD before handoff** `8a010bf63a29e3697dd757ed6b50c02a84fe2783` (v2.3.0/V359 ancestor `ef87613c1df8b7eccb44620dbf7447f0b5bdd37a`). [Win64 CI 39840976835](https://github.com/julkszw-sys/DSRRL-Material-Response/actions/runs/39840976835) PASS, PE size 2,369,024 bytes SHA256 `33d8a75fbd7f960e88ef51596b0b70f3046af4e0806dfa11de510a61dd823672`. **V370 runtime/Quelaag pixel OPEN**. Release protected V359-RC1 unchanged.

**V364** draft [PR495](https://github.com/julkszw-sys/DSRRL-Material-Response/pull/495), HEAD `4878d9b712b27f80c67c449cbf1187bdaf6b544b`, is the ALREADY WRITTEN exact *legacy resource* bridge. Its owner log `ReShade(20261022-063522).log` proves RX34 routes160/88 actual t11 `EnvDif_m18_500`, 20/20 independent PTDE donor RESOURCE_READY, but t12 EnvSpec rejects downstream and no shader/pixel proof. `EnvDif_m18_510` creation name+SHA proven but no bound t11 in that particular log. Reuse; do not rewrite.

## Original complete operator (do NOT replace with V365-V369)

`E_PTDE = (filtered_raw_t11_RGBA.rgb / filtered_raw_t11_RGBA.a) * c86.rgb`. Old 29-DWORD producer in `renderer-core/src/operators/env_spec/pmetal_rgba_materializer.cpp::apply_exact_ptde_envdiffuse_consumer()` replaces DSR `sample_RGB(t11) * cb0[3].rgb * cb0[79].x`. V370 extracts **that same original 29-DWORD source** via `materialize_ptde_envdiffuse_stable_only`, introduces fresh r13, exact wxyz sample swizzle, local b12 declaration/RDEF; `pmetal_envspec_draw_runtime.cpp::prepare_independent_envdiffuse` uploads `source.envdiffuse_a` to `cb12[3].xyz` and private PS/t11/s11/CB12. `integrated_addon.cpp` gives Stable RX33/34/35 priority then fail-open stock, never to old coupled island. Lerp two-endpoint is NOT isolated. Non-P_Metal coverage is NOT globally proven.

**Missing:** `envspec_resource_runtime.cpp::prepare_envdiffuse_bound` still resolves only `g_diffuse_by_view` PackedGI 342. Legacy needs exact native COM t11 backing + original TLS `EnvDif_m18_500/510` name + creation SHA + device/epoch, to the exact original PTDE donor. V364 has `renderer-core/include/dsrrl/operators/env_spec/legacy_envdiffuse_m18_exact.hpp`, `tools/materialize_v364_legacy_envdiffuse_source.py`, `tools/materialize_v364_legacy_envdiffuse_cube.py` and earlier V362/V363 registry. **Port their semantic implementation to V370**, do NOT blindly run V364 generators on V370 checkout (source anchors may differ). Native legacy GI uses independent record offsets `+0x50` EnvDiffuse, `+0x52` EnvSpec. NO t12-derived ordinal or guessed source alias. Make resource compile guards depend on standalone V370 feature instead of the old `DSRRL_PMETAL_FULL_PTDE_HEMENV_DIAG`, without re-enabling old coupled shader.

## Exact assets already available in portable ZIP

- `DSRRL/EnvDiffuse/Legacy/PTDE_LEGACY_ENVDIFFUSE_ALL_MAPS_DDS_V1.bin` — original **39** PTDE EnvDif DDS, `DSRLEDF1`, 820,208B SHA `3be05fffacfbc0af3f5a4124f0f79694c20480057b3a5c76470667ed712a6b47`. Independent original 39-record verification PASS. Layout header `<8sIIQQ`, 39 records `<40s6HIQQ32s24s`, offset32/data offset5024. This asset is **not read by V370**; all 39 runtime identities NOT yet proven.
- `DSRRL/EnvSpec/PackedGI/DSR_NATIVE_ENVDIFFUSE_PROBE_HASH_V1.bin` — 12,336B SHA `cd3549ee84410f932909e818f07d5009347314f991bf6b0c69bffa46fceb747c`.
- `DSRRL/EnvSpec/PackedGI/PTDE_GI_ENVDIFFUSE_PACK_RGBA.bin` — 8,404,992B SHA `5ab4a471fedbbc95d8dceb55ed0f4648582a824b4290bec03e6134d7cb81cd8c`.
- Two **exact V364-compatible converted** original PTDE m18 donor binaries in portable ZIP `DSRRL/EnvSpec/LegacyGI/EnvDif_m18_500_PTDE_RGBA8_CUBE.bin` (24,576B SHA `f1f6b147291e0648e219bf8c6475f74743d22c66b7f885fe547bdb98ab4542b4`) and `EnvDif_m18_510_PTDE_RGBA8_CUBE.bin` (1,536B SHA `7c9af74457b4a488349e117c9eafafb66881142702186545f8906d43ca6c4426`). Converted byte-for-byte from original BGR24 DDS; hashes exactly match canonical V364.
- Native DSR `EnvDif_m18_500` BC6H creation SHA `19241a18f938ea8fa00dcaede57e9116c60cf818d5c8faaa1cc93e7eb98ce81b`; `m18_510` SHA `cace4d9bdbe2bd54bb9b4e7c91002b88c6a68c367638febde9c76144ee5a7eb9`.

**Rules:** verify exact 39 DDS records and DSR runtime ownership; only the 2 m18 native names currently have exact donor joins. Remaining 37 asset bytes are verified, but runtime native mapping OPEN. BC1 `EnvDif_def_000` has 7 mips; preserve original BC1/alpha behavior, no universal forced alpha=1. DO NOT alias legacy to PackedGI or to EnvSpec `t12`. Lerp mixed producer classes fail-open until independently proven.

## Required next proof

1. exact compiled private shader RX33/34/35 and verified CB12 binding, actual user game `CreatePixelShader` (not CI only).
2. positive source tests PackedGI + m18_500 + m18_510 and negative name/SHA/epoch/view/sidecar/dup/ambiguous/unknown/beta/receiver tests.
3. new per-frame/actor/receiver/material/t11 class/CB12 GPU issued+restored logs — old V369 EDC RX35-only metric is biased by early return.
4. one Windows single addon **with required sidecars and SHA manifest**, owner game runtime and PTDE pixel compare. No pixel PASS on build/ready logs.

Read the full portable ZIP first, then update this index and Supabase evidence. V365–V369 partial new EnvDiffuse shader is REJECTED as complete PTDE consumer. **Don't waste owner tests repeating that mistake.**
