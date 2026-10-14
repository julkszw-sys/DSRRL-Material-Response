# V274B — owner runtime CPU CB0 producer witness, 2026-10-14

## Exact provenance

- Owner uploaded `ReShade(20261014-211633).log`, SHA256 `b61608552543c59c6ce162be00ad4481308e6903fa4add29116ab6df88b3641f`, 1482 lines, 515913 bytes.
- ReShade 6.8.0.1 API20. `BUILD_ID source_commit=8474d5691d6a140562577b02189d3a020a57a3f4` matches V274B source; original stable is V272 source `ae9ac314d06ad8758e1aa7430cb236eef0eefe27`.
- New `[DSRRL V274B DEFERRED CB0 CPU SHADOW]` entries **96**. Original V274 readback still **96/96** `cb0_read=0 reason=DEFERRED_CONTEXT_NO_MAP`. Never use those V274 fallback zeros.

## Actual CPU writer snapshot findings — sampled prepared draws ONLY

| Measurement | Observed |
|---|---|
| RX33 / RX34 | 58 / 38 |
| `cb0_bound=1, shadow=1` | 96/96 |
| `CPU_WRITER_SAME_THREAD_ORDER_ONLY` | 96/96 |
| `writer_source=1` (map/unmap) | 96/96 |
| `ctx1=1 first=0 count=4096` | 96/96 |
| `cb85.x` | **0** (96/96) |
| `cb100.w` | **1** (96/96) |
| `cb102.x` | **0** (96/96) |
| `update_cb` | 0 throughout 96 retained samples |
| Beta sampled | ~0.033333335, 0.26666668, 0.5, 0.76666689 |

### Exact route/MTD/slot-stratified producer values

| Route | MTD hash | samples | `cb0[10].rgb` | PTDE c101 |
|---|---|---:|---|---:|
| 160 | 73ec84781e69fdc8 | 36 | 1.20000005 ×3 | 1 |
| 232 | 0a970c711b27e2cf | 15 | 0.699999988 ×3 | 1 |
| 1073741869 (ordinary leather) | abbcca95a57070c3 | 15 | 1 ×3 | 1.5 |
| 345 (P_Metal) | fd72a0409ae13e45 | 20 | 1 ×3 | 2.5 |
| 88 | e3d219814a0bd57e | 10 | 1.20000005 ×3 | 1 |

Material-specific `cb10` stays invariant within recorded beta bins.
All logged `flver=0000000000000000`; this prevents promoting full physical FLVER/slot identity. MTD hash/route/slot exist, but not complete tuple.

## Semantic operator implication

V277 confirms actual R19 final PS has `IF_Z cb0[85].x`; stock DSR second gamma EXP `pow(saturate(SpecTex.rgb*cb0[10].rgb),2.2)` is in NONZERO `ELSE`, not zero IF branch. All 96 Lerp prepared CPU writer snapshots therefore **select zero branch under producer-state assumption**, indicating second material gamma is not the cause in these covered preparation samples. **NOT proof of actual GPU conditional execution**: deferred CB can be mutated after preparation; command list execution state was not GPU-read.

The R15 DSR-only `Wextra=(1-SpecTex.a)*cb0[100].w*10` lives in zero IF branch of original stock Lerp. Thus `cb85=0` does not rule out original DSR additive/PBL operator, especially in rejected V273, whose broad stock workflow restoration made P_Metal brighter. V272 keeps R15 neutralized. The SpecTex alpha for individual observed draws was not measured.

V272 interior `b12_spec_w3` and `b12_diffuse_w5` exactly equal source beta in **8/8** PREPARED observations across 0.0333333351→0.900000334. This is host/producer routing, **not** pixel pass.
Runtime: 31 `V251 LERP NATIVE` before_draw exact route345 deferred CB/SRV/sampler valid, 2 `V250 PMETAL LERP DRAW` DRAW_ISSUED (RX33/34), and Lerp MR diffuse draw_issued stage. These are separate callbacks and do not prove all 96 CPU shadow snapshots issued.

## Next operator-level decision

- Stop using V274 GPU failure zeros, V273 rejected restored stock workflow and global second-gamma hypotheses as visual fixes.
- R5 spatial normal bitangent producer is independently source-level different between stable R17 `normalize(cross(N,T)*tangent.w)` and Lerp R19 stock `vB` with `SV_IsFrontFace` NEG (V281). This is **a separate potential first operator mismatch**, not yet a pixel root cause.
- To test an R5 operator island, verify exact RX shader SHA, VS physical semantics, PS constant ABI, FLVER/MTD/material slot and both texture ownership, activate **Lerp only** and fail-open others, preserve all V272 stable, Fog/Glow and PointLight. Do not merge PR#364 globally.
- V274B should be removed from production after diagnostics; CPU map/unmap counted >=866001 CB maps during sampled session, a nontrivial hot-path overhead.

## Status separation

**Construction PASS; runtime producer snapshot MATCH 96/96; true consumed GPU CB and branch OPEN; spatial-normal residual OPEN; PTDE pixel improvement OPEN.**
