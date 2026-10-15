# V275 — Exact Cloth/Leather draw-state diagnostic (V272 production behavior frozen)

**Scope:** V272 source at `ae9ac314d06ad8758e1aa7430cb236eef0eefe27`, extended only by diagnostic callbacks from V274B/C; one addon. No EXE, DXBC, output color, Fog, LS, Glow, U/L, or material source manipulation. This is the **routing and first-consumer prerequisite**, not yet an in-shader GPU pixel probe or PTDE pixel fix.

## Install and collect
1. Back up the existing working DSRRL .addon64. Use **only one** diagnostic addon in the ReShade addons directory. Restore the known-good addon after the session.
2. Launch game with V275. Find `[DSRRL V275 MATERIAL DRAW]` and `[DSRRL V274C ATMOS CB0]` in `ReShade.log`. If neither appears, classify as NO_RECEIVER_HIT / NO_MATERIAL_SESSION, **not** as material PASS. First check ReShade add-on load.
3. Choose a known **BAD leather/cloth** part, a nearby **GOOD leather/cloth** part, and **P_Metal control** in the same scene. Avoid editing uniforms by hand or changing equipment during a single A/B state sequence. Keep same camera/pose if possible.
4. Record **Stable A**, one or more interior Lerp states (around beta 0.25, 0.5, 0.75), and **Stable B**, and take matching screenshots or capture graphics frame. Beta labels require authentic producer witness; the logged `fog12w/fog_bucket` is **atmospheric state only**, never infer physical beta from it.
5. Close game and run:
   `python renderer-core/tools/v275/analyze_v275_draw_log.py ReShade.log --out v275_report`
   Optionally label exact **full** 64-digit MTD SHA via CSV header `full_mtd_sha256,status` with `GOOD` or `BAD` and `--labels labels.csv`.
6. Review `outlier_matrix.csv`, `coverage.json`, `groups.json`. A missing material is **not a working material**, but a missing authenticated sample (possible fail-open outside registered receiver). The 768-row limit is intentional.

## What the addon observes

V275 inserts callbacks into both `draw` and `draw_indexed` at the authenticated material+receiver junction, source-preserving. It logs:
- full raw DSR MTD SHA and full FLVER SHA, semantic hash and slot, physical owner tuple and exact parser material identity;
- exact 27 Cloth/Leather canonical MTD names plus P_Metal control, identifying SHA collisions and any name-only/raw-only partial matches;
- Stable/Lerp branch (current receiver), RX, MR decision/reason/c100-derived material binding, c101/c102 PTDE authoring;
- **D3D11 host-bound** PS interface identity, t1/t10/t12 SRVs and formats, RTV0/RTV1 identities;
- **planned** atomic island PS replacement and t10/t12 overrides BEFORE actual replay, independent of host;
- V274B/C prior CPU-writer b0 producer cb85/cb10/Fog c12.w state with explicitly non-GPU evidence label;
- `IDENTITY`, `PREPARE_FAIL`, `BATCH_PREPARED`, `DRAW_ISSUED`, `DISPATCH_FAIL_OPEN`, `RESTORE_FAILED` stages.

Importantly, `DRAW_ISSUED` certifies a draw transaction call, NOT which values entered its GPU execution and NOT PTDE pixel equivalence. No full RTV readback is attempted inside D3D11 deferred contexts; previous V274 staging `DEFERRED_CONTEXT_NO_MAP` proved that approach non-authoritative.

## Targeted GPU follow-up / RenderDoc

For a **single** material where both Stable and Lerp identities are logged:
1. Capture Stable A, Lerp interior and Stable B with identical camera and equipment using a D3D11 frame debugger (RenderDoc if the active ReShade hook arrangement captures faithfully). Verify which exact PS is executing: original vs V272 RX shader replacement. If ReShade hook is not visible, **do not use** an apparent stock capture as a bridge falsifier.
2. In frame debugging identify the indexed/nonindexed draw via vertex/index count, RT dimensions, resources, and PS. Select an affected pixel. Use Pixel History to check if extra draw/blending explains laminate BEFORE entering shader RE.
3. Trace PS instruction/registers at **t1**, **t10**, MaterialWorkflow liveout, EnvSpec, first preFog value, postFog, postLS, final postpow and RTV. For simultaneous RT0+RT1 inspect both.
4. Export matched GPU snapshots with named stage, material SHA, FLVER/slot, exact PS SHA, t1/t10 logical resource identity and probe A/B. PTDE source A/B only after verified semantic match.
5. First divergent stage dictates narrow carrier: source/resource/CB if input wrong, shader/material response if preFog wrong, postprocess only if first difference later. If unresolvable, fail-open to stock DSR; do NOT add a gain.

**Status matrix:** single-addon Windows compile TBD until linked CI pass; source construction CI TBD; owner runtime OPEN; physical draw identity OPEN until the owner's log; GPU/first-divergent operator OPEN; PTDE pixels OPEN. CI binary SHA is build reproducibility only.

## No repeat of rejected fixes

V273 restored stock MaterialWorkflow broadly during Lerp but owner rejected pixel outcome; V274 CPU witness cannot establish actual GPU branch. PTDE original 24 noPointLight Lerp shaders lack live SpecTex.a reads: do not restore the DSR alpha gate globally. P_Metal route does not certify nonmetal behavior. V272 stable A/B and DSR independent atmosphere are protected.
