# V279 COMPLETE HANDOFF INDEX — 2026-10-15 — canonical Supabase rev. 13212

**Repository lineage:** V272 frozen source `ae9ac314d06ad8758e1aa7430cb236eef0eefe27` → diagnostic V277 → V279. **V278 is separate, negative experimental R5**, not inherited into V279. Build V279 source exact commit `2c6f992ae00ad9107f34c85959ed87d12bdc50e7`, PR #388, Windows CI PASS [38686192843](https://github.com/julkszw-sys/DSRRL-Material-Response/actions/runs/38686192843), CI-reported binary SHA256 `14846917023bc67bae4f9305fdcbda762ad49ef56add51edb1aa9af757c54096`, artifact ID `11950801354`. Prior canonical 13209 named a different *older* V279 CI artifact [38685456882], binary SHA `c5cb7ec6dd3c4feb34557ed940d619f02ed2829eaa5a3bd6c74ae7e487c24a60`; do not mix.

**Owner phenotype:** P_Metal[DSB] visibly correct during HemEnvLerp, P_Leather[DSB] glossy laminate during Lerp, no pixel/FLVER-slot BAD annotation. R5 source asymmetry was proven but owner V278 negative A/B and create-time `R5_APPLIED` all RX33/34/35 plus 216 issued Lerp draws, so **do NOT treat missing R5 as sufficient cause/fix**. Supabase 13199.

**Source pair verification:** Owner V278 log SHA256 `eb201176129ff8c8c22ee1d2b0ca65f28913bd8c1c4c01ad91a6f3608c822cf6`. Two exact FLVERs:
- `eb3b77ee419f9e2997b7d427987c002411fdf08edda12834be4a9f51b3c4eb7b`: Leather slot2; Metal slot3.
- `672e35606495f24e36c704f32fceda9b933563a64e03379375f47f9fe04a0a7d`: Leather slot8; Metal slot5.

18 matched `(RX,fog_bucket)` Lerp `BATCH_PREPARED` per FLVER, **36 source pairs** total, 28 RX33 and 8 RX34, **14 distinct fog buckets**. In all pairs: identical host/replacement PS object, host t1, PTDE SpecRGB t10, host t12, RT0/RT1, CPU cb85/cb10/fog12w, t10/t12 mutation activation flags. PTDE EnvSpec replacement t12 resource object **differs** per material, as expected from different EnvSpec slots (Metal2, Leather1). V229 original sparse Gloss Cut c101 Metal2.5/Leather1.5, specpower Metal8.5/Leather4, probes A/B distinct and EnvDiffuse common. **V275 Metal c101=0 is control placeholder, not physical value.** The misleading filename `V279_METAL_GOOD_LEATHER_BAD_MATCHED_18_LERP_STATES.csv` actually contains **four** source rows, *not* 18 records or a full 36-pair table. Canonical 13212 captures verified 36 source pairs but GPU/pixel still OPEN.

**R8 SPEC-domain shader-source gap:** Original owner V277 DXBC capture ZIP SHA256 `fcb182f4569229b8985743d1068e89b198723d3b2d97b4200437ad6298605981`, exact 12 shaders RX33/34/35 Stable/Lerp Stock/Bridge. All 3 Stable STOCK contain exactly one `LOG→MUL(2.2)→EXP`, all Stable BRIDGE **zero** after R8; all Lerp STOCK and BRIDGE **one** (3/3). Lerp also has `IF b0[85].x` branch: true branch 4 changed, false branch 2 changed instructions, including coefficient from `b0[9]` to `b12[1]` and direct RGB factor `2.2→1`. Despite CPU `cb85=0` shadow, **physical GPU branch and preFog/EnvSpec result remain UNKNOWN**. This asymmetry is SOURCE confirmed, not R8 causal proof; protect correct Metal. Supabase 13203,13206.

**What V279 actually does:** read-only exact four owners (two MTD SHA / two FLVER + four material slots), RX33/RX34, Stable/Lerp, logs the mutation shader, operator owner masks, prepared CB, t10/t12/t13/t14 SRV, s10/s12/s14 samplers, host pre-CB12/t12/t14, source CPU shadow. **No actual GPU CB physical readback, no pixel sampling, no shader math change**. No owner runtime log V279 exists. Source:
- `renderer-core/tools/v279/v279_material_carrier_trace.hpp`
- `renderer-core/tools/v279/apply_v279_material_carrier_trace.py`
- `renderer-core/tools/v279/build_v279_one_addon.ps1`
- `renderer-core/tools/v279/analyze_v279_owner_carrier.py`
- `renderer-core/tools/v279/V279_GOOD_METAL_BAD_LEATHER_FIRST_CONSUMER_PROTOCOL.md`
- `renderer-core/tools/v279/V279_EXACT_3RX_STABLE_R8_VS_LERP_SPEC_POW22_GAP.md`

**Protect:** per whole-project `protect.project.no_new_capture_or_runtime_test_dependency`, **no future solution may REQUIRE new captures or user runtime tests**. Use historical 12 DXBC, original PTDE files in owner's private archive, existing logs and offline dataflow analysis; if target cannot be identified, report OPEN/residual rather than guessing. Preserve exact material/slot/receiver gate, independent DSR PBL/LightScattering/Fog, source-complete Git builds. Do not promote V279 construction to runtime/pixel pass or impose a blind global R8 gamma edit.

**Next actionable RE:** offline walk actual Lerp `LOG-MUL(2.2)-EXP` result register to first consumer, compare to Stable R8 and original PTDE matching shader/operator; determine reachability under b0[85] via available *existing* source/captures, and whether Metal GOOD and Leather BAD respond differently to material alpha or probe. Only if responsible first mismatch verified consider narrowly gated A/B, then pixel test if possible, without blocking work on new owner capture.

**Status:** SOURCE R8 CONFIRMED, material source pairs 36 CONFIRMED, owner visual phenotype CONFIRMED at family level, owner V279 runtime OPEN, physical GPU first consumer OPEN, PTDE pixel OPEN. **No shipped visual fix.**