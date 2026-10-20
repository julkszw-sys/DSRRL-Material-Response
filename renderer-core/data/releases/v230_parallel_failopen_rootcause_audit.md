# V230 parallel fail-open root-cause investigation — 2026-10-20

**Source**: exact diagnostic-only descendant of `3f226d5d2e583317c16486361f53ced7fbb33e01`, inheriting V359 RC1 → v2.3.0, stock DSR EnvDiffuse, and a14/m14_LightBank.param row6 EnvSpec intensity **170**. Single compiled ReShade addon.

## Owner runtime evidence

Owner-provided ReShade log (16:54:40–16:57:14) verified shader Stable+Lerp RX33–35 creation; final at 16:57:14: EnvSpec prepared 157341, probe rejects 17490, SpecRGB rejects 2606, early LightBank source reject 947, MR decision inactive 133829, draw identity and owner rejects each 958866 (two observations of the same rejected draws). A prepared request is *not* an issued draw or a PTDE-equivalent pixel. Four primary tracks are handled as independent operator-local branches; no generic pass-through.

## Independent diagnostic work streams, same addon

1. **Probe resource (t12/t14)**: `snapshot_ready_envspec` now returns an out-of-band diagnostic stage for missing stock slot, device, PackedGI pack, sampler, unrecognized stock SRV A/B, ordinal out of range, missing PTDE cube A/B, native object null. A missing cube is still subject to the same exact donor/pack resource loader. Failures during recovery get their own stage. The additional out parameter cannot grant resource authority. The per-stage `[DSRRL PROBE ROOT]` and `[DSRRL PROBE ROOT SUMMARY]` markers identify the stage rather than the uninformative previous composite `detail=30`. Logs are outside the resource mutex.
2. **Material/SpecRGB (t1)**: `[DSRRL SPECRGB ROOT]` is emitted once per receiver/family/route class on an actual reject. Reports `material_ready`, `prepared.spec_rgb`, number of retained requests, resource shadow status, owner tuple/MTD provenance, t1 binding and logical hash allowlist/companion resolution. Cannot assume that `actual_material_exact` permits use of an asset requiring `owner_tuple_exact`. Existing exact SpecRGB consumer SHA/resource/MTD gates remain.
3. **LightBank / Ceaseless donor**: `[DSRRL DONOR SOURCE]` reports the actual source consumed (bank signatures, rows, A/B RGB, beta). `[DSRRL CEASELESS ROW6 CONSUMED]` reports only if source bank signature `c9b879a8c5e624d0` row6 actually reaches the shader path. Compiled list entry 170 alone is not evidence of runtime activation. Original PTDE donor and independent systems remain unchanged.
4. **MR and shader/draw routing**: `[DSRRL MR ROOT]` reports receiver, reason, owner_tuple_exact, actual_mtd_exact, raw slot, semantic hash, and route; distinguishes `ptde_companion_required=4` from `unknown_material=5`. Existing exact shader-create result 4 is **fail_diffuse_base**, a separate code space. `native_wrapper_armed` is explicitly distinguished from `draw_issued` and `draw_transaction_reject` so native wrapper draw path is not incorrectly counted as failed replay.

## Construction vs runtime

No broader bridge activation; no replacement of `DarkSoulsRemastered.exe`; no global deactivation of independent DSR operators. All new program logic is diagnostic logging or out-param reason classification. Tool scripts V354/V357 are still required to pass before CMake, and CI must pass native Windows build. These changes require the next owner's runtime log for *actual* first root stage, receiver attribution and donor activation. PTDE pixel and frame performance remain **OPEN**.

### Decoder

- Probe reason 1 invalid stock/slot; 2 no device; 3 no pack; 4 no sampler; 5 stock A SRV unmapped; 6 stock B SRV unmapped; 7 probe ordinal out of range; 8 no PTDE cube A resident; 9 no PTDE cube B resident; 10 null native object; 11 materialization retry failed
- Probe initial-stage records the stage before attempting `get_ptde_cube`.
- `[DSRRL SPECRGB ROOT]` failure with `owner_tuple_exact=0` must **not** be bypassed without verifying the asset's exact FLVER/MTD and bound t1.
- `[DSRRL MR ROOT]` reason4 is a required PTDE companion, not a request to force `decision.active=true`.
- `[DSRRL CEASELESS ROW6 CONSUMED]` is a semantic/CB signal observation, *not* pixel equivalence.

The previous user log lacked all these deeper fields. Logs will be bounded by reason and receiver.
