# ARMOR LIGHTBANK FLICKER — corrective source-of-truth handoff (2026-11-06)

**Scope: DSRRL Renderer Edition / Source1 / RX34. No addon build is implied by this doc branch.** The complete owner handoff is also provided in the corresponding conversation as `DSRRL_ARMOR_FLICKER_CORRECTIVE_FULL_HANDOFF_2026-11-06.zip`, SHA256 `af1234cd8850d4761f3c4d9c7bfeac43ba815d0b2d121ece7a9e5b040f68dca8`. Follow its `START_HERE.md` and `OWNER_CORRECTION_CHECKLIST.md` before any patch.

## CRITICAL: correct parent selection
Supabase control plane at revision 14758 identifies **frozen ACTIVE_DEVELOPMENT root V359-RC1** source commit `6e852cdc9085dd175159e564e223e0cc8589c535`, RC binary SHA `7d971d07dba6143bec1d5b5ea1811cceb6cd07f06fa3c0bae0599b9b0bf2d532`. This is immutable owner-authorized root, **not** permission to reinstall it and discard 635 later commits.

The **last actual flicker diagnostic .addon64 tested by the owner** is **V439** branch `agent/v439-auto-safe-flicker-runtime-20261105`, source commit `1f6bc31c1b28755ed226d24de51717aba6ce8184`, binary `DSRRL_V439_Auto_Flicker_Safe.addon64` SHA `d03ed99fef575fda4b6346f0a4f3af4e9a95fba8e787254f5b36e7b3bb9db274` (5,498,880 bytes), Windows CI run **42092935869** PASS. Git compare V359→V439 = ahead635/behind0; V359→V442 = ahead673/behind0. The later V440/V441/V442 branches add **static research only**, NOT verified new addon runtimes.

**For this same isolated flicker diagnosis**: branch from the **V439 source** while preserving V359 upstream ancestry, retaining existing safe packer/source/draw observers and selectively importing V440–V442 verified RE. For future broad releases explicitly revalidate which V439 diagnostic runtime policy disables (PointLight draw-time / HemDir3 / Subsurface / U-L visible bridge were inactive in its log), so these disables do not leak into shipped release. Do not build from naked `main` (`410e4591...` at handoff), V438 crash experiment, V350 rollback, or unbuilt V442 research as if it were tested runtime. One reproducible integrated `.addon64`, immutable RC root.

## PROBLEM ALREADY KNOWN — no rediscovery
Owner's direct scenario: **character standing still, camera rotating, armor visually flickering**. V413/V414 had already proved rapid LightBank row30/31 changes; V414 canonical revision 14612 documents shared `FrpgDrawContext` with 19–20 FLVER assets, not an actor identity certificate.

Original V439 runtime log `ReShade(20261105-193253).log` SHA `b56d09d7f26de2dc86f8c41d95ba093edf4c663c9684c780c639339dad624e7d`:
- Candidate model pointer `0xE979970` **NOT YET proven to be actual visible armor**, FLVER SHA `ed0a79b086f1cff26005e0d2c184e269b532046d7cfb83d1eda3bcc3911ac6cc`, MTD SHA `ece70f36bd2517d28c8495e276cea537f8b519d6bed981788e79a409ffbf763b`, RX34 route345 shader `0x9A14DA78`.
- V418: 114 event-triggered samples, 57 per slot (0/1), 53 row changes among 56 neighboring LOGGED samples per slot (NOT 53 frames). V409: 48 issued samples, row30=22, row31=26.
- Row30 native A/B=1310/1310, beta0, C86=(1.2,.8,.6588), probe242. Row31 A/B=1311/1311, beta0, C86=(.4,.4,.4), probe243. Both source kind1.
- **Every one of 114 V418 samples has `exact_renderable_join=0`**, actor identity / GPU CB / RTV / visible pixel OPEN. Row changes are the **known direct flicker mechanism**, but whether this is original-game selector misbehavior, Source1 cross-actor misassociation, or PTDE vs DSR consumer mismatch remains OPEN.

**Do not call the phenomenon undiscovered**; do not present row30/31 as fresh RE. Do not assume even a verified model pointer is actor generation. Avoid drawing pixel-causality conclusions from event-triggered samples.

## V438 fail and V439 behavior are hard constraints
V438 binary SHA `1b6091df0e0940e20ac3745d65841bba4bda112a1c2193201e7f4190d9314dfd`, source commit `3901e608dd957e7b46c94bfa31c64a949e9ed812`, user-reported crash while new 17-byte native type5/type6 wrapper detours active; no exception RIP, so exact crash cause OPEN. **Do not re-enable these unsafe detours.** Original V438 wrapper vtable guard checked only `FrpgModelDrawEntity`, missing legitimate sibling `FrpgAsmModelDrawEntity`, another confirmed coverage gap.

Owner explicitly removed **F9 requirement**. V439 starts capture automatically after existing V415 model selector activity and presents; F10 is OPTIONAL. Legacy log prefix `[V438] CAPTURE_START AUTO` and folder `%TEMP%\DSRRL_V438\` are inherited names only; `DSRRL BUILD_ID` proves V439 actual binary. V439 owner session had ~67s capture start without completed 180s session; it does not prove universal crash-free or pixel behavior.

## Native RE useful only for this missing identity edge
- Two sibling native entity classes, distinct dispatchers/builders: `FrpgModelDrawEntity` vtable 0x12ECA10, builder 0x2200D0 packet `RBP-0x30`; `FrpgAsmModelDrawEntity` vtable 0x12EBFE8, builder 0x2162D0 packet `RBP+0x20`. Both embed `FrpgDrawParam` at actor+0x108. FIRST LightBank channel actor+0x110 has WORD A/B, float beta/duration/elapsed; setter 0x1D7820, tick 0x1D77E0, triple setter 0x1D7580. Setter can also modify unrelated structures; validate current RCX/owner before inference.
- Default builder calls type5 at 0x220639 or type6 at 0x220632, virtual model draw 0x220CED; ASM calls type5 0x2167BA and model draw 0x2172AA. Original emitter type5 holds dp in callee-saved R14, type6 in R15; existing packer single/blend 0x563B80/0x563C30 already hooked. Win64 original pdata chained unwind can (in principle, NOT YET live) recover same builder frame/actor/packet. Do not infer frame join from matching Source1 serial/TLS/last producer.
- Typed model 0x20DDA0 reads packet A/B at +0x4C/+0x4E and dispatches shader selector 0x22BA20. Camera-only selector writer provenance remains OPEN. Exact typed model->issued RX34 remains OPEN.
- V442 geometry operator 0x1C08E0 produces indexed 3x4 matrix/skinning palette `FrpgDrawContext+0x3E0+0x30i`, not LightBank row. **Do not divert RE there** without direct receiver evidence. Two additional DrawParam channels also have separate CPU consumers and resource semantics; no proof they are causing this issue.

## Owner corrections binding to this and related scoped cases
1. Solve **native producer → actor/renderable generation → context → material/FLVER → issued RX34 GPU**; don't spend days re-proving row30/31 or exploring unrelated palette/DoF/PointLight.
2. Keep `latest_hook_source()` and R43 TLS/global Source2 fallback; owner-protected. Restore/do not arbitrarily suppress Source1 kind1; preserve working P_Metal and PTDE EnvDiffuse sidecars. No global row/gain/brightness workaround.
3. **Not only P_Metal:** other armor material families (e.g. C_Metal/Edge) and RX35 cutscene cases need exact FLVER/MTD/slot/shader/asset SRV routing. Do not generalize one RX34 model to all equipment. Existing one-time Quelaag scene must NOT require replay/new game.
4. Native selector `-1/-1` means inherited state where verified, NOT row0. No forcing row5, no reverting to R44 black armor or unqualified stock fallback; earlier experiments were falsified. Cutscene black armor and camera-only row30/31 are **distinct until proven shared**.
5. No guess of probe/SpecRGB/EnvDiffuse sidecar or shader gamma, no mapping by prefix/hash alone; if bridge's logical SRV/MTD/receiver unknown, fail-open scoped. Do not audit all 641 PTDE shaders: test changed DSR shaders and exact PTDE counterpart with full D3D11 state restore.
6. PTDE-visible behavior is the target, DSR the host, Switch the reference. Keep PBL/TAA/SSS/PointLight/DoF independent. No EXE disk modification. No unproven source/pixel promotion.
7. Never request another test that only reproduces known row switching. For any proposed test, state the exact competing hypotheses and discriminant data. Unnecessary F9 / repeated one-shot capture is forbidden. Bootstrap/status and PROTECT before work; evidence+finding+revision+knowledge hygiene after.

## Exact next discriminant
1. Identify *actual visual armor* (not guessed `0xE979970`) and actor generation for each sampled draw.
2. Observe for same actor generation native dp A/B **before** emit, including who called setter; cross-check model builder packet.
3. Prove same native emitter invocation and typed FLVER/MTD material reaches issued RX34 with exact CB/SRV identity (using bounded validation through already-hooked packers, NOT V438 wrappers).
4. If actor dp native selector flips: trace original DSR setter upstream and PTDE corresponding rule. If dp stable and RX34 flips: repair Source1/owner/cache join. If both agree but PTDE response differs: exact receiver/material/resource operator port. No canonical patch before this fork.
5. Only build next one-addon when a **new** discriminant is instrumented and verified; status reported separately for construction/compatibility/liveness/activation/pixel.

**Supabase links by key:** `renderer.v414_flvermodel_typed_draw_precontext_identity_cut_v1`; `renderer.v439_rx34_same_model_row30_31_c86_probe_native_selector_discontinuity_v1` (known replication, not discovery); `renderer.frpgdrawparam_three_channel_native_a_b_beta_duration_elapsed_state_v1`; `renderer.type5_type6_emitter_exact_drawparam_r14_r15_chained_win64_unwind_v1`; `renderer.frpgdrawcontext_exact_post_lightbank_prepass_and_three_model_receiver_mutation_v1`; `renderer.indexed_submodel_transform_palette_remap_3x4_context_geometry_v1`; PROTECT `renderer.pmetal_env_source.latest_hook_source`.

**Status as of handoff:** source/operator static RE scoped CONFIRMED, known issued row switching CONFIRMED, V439 build PASS / runtime sample observed, native actor-to-issued join OPEN, exact culprit OPEN, PTDE pixel OPEN. Do not change this status without direct evidence.