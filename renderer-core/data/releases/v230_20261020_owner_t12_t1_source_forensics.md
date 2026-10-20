# v2.3.0 owner runtime — 2026-10-20: source-identity frontier

Evidence: `ReShade(20261020-153142).log` SHA-256 `f84fa97d9580eb3fe8d20244f76325941137a7ee90043870c39affbfde1fd7c7`. 182 lines, final summary at 17:30:57. All counts are **attempts**, not unique meshes.

## Confirmed execution and unresolved gates

* Verified donor `a14:m14_LightBank.param:bank8:row6`: exact signature `c9b879a8c5e624d0`, `rgb_a=(1.7,1.5666666,1.0333333)`, `rgb_b` equal, `beta=0`; logged `SOURCE_READY` then `REQUEST_READY` for RX33 route345 and subsequent `draw_issued` on same thread/receiver/route. This proves signal consumption and corresponding issued draw but **not pixel equivalence**.
* Three stable + three Lerp receiver shader creations OK (RX33,34,35).
* First LIVE summary: `pmetal_prepared_ok=19427`, `draw_issued=19427`. This means active drawn transactions, not necessarily all EnvSpec output pixels matched PTDE.
* 1594 probe fails: `probe_snapshot_reject=1594` and `[DSRRL PROBE ROOT SUMMARY] stock_t12_unmapped count=1594`. Exact gate #5. First example RX34, route160, material slot3, **EnvSpec logical slot0**. Do not conflate these two slot namespaces.
* 200 SpecRGB fails: RX34, **exact P_Metal** route345/material slot2 first witness: `material_ready=1`, `spec_request=0`, `requests=0`, `shadow_t0_t2=0`, `owner_tuple_exact=1`, `actual_mtd_exact=1`, `bound_t1=1`, `logical_known=0`, `name_allowed=0`, `companion_ready=0`, `t1_hash=0`. A resource bound to t1 is **not** enough to authorize PTDE SpecRGB without a certified source/sidecar.
* 53269 MR inactive attempts; exemplar reason4=PTDE companion required on RX33/34 with `actual_mtd_exact=0`. These are protective, not blanket MR defects.
* `draw_identity_reject=114690` and `draw_owner_reject=114690` are two observations of the same population; do not sum as two disjoint failure classes.
* In this short run no first-hit `source_reject` or shader materialization failures. Prior session startup source misses do not generalize to this run.

## Narrow follow-up construction

Only on bounded rejected RX/family/route classes, inspect *actual native* t12 or t1 PS SRV for exact same D3D11 backing, existing V358 creation SHA/lifetime/certified ordinal and existing V214 exact original retail source-name carrier. Emits `[DSRRL V230 MISSING SOURCE]` with `action=FAIL_OPEN_NO_ALIAS`. No new material authorization or probe map, no shader patch, no texture replacement, no global gain. Unknown resource never mapped to an inferred PTDE probe. Capture original view shadow provenance separately if native state differs.

Decode:
- `creation_data_exact=1 native_probe_certified=0` and `sha256` available => exact captured BC6H bytes but atlas match missing. Needs independent exact asset map/evidence.
- `creation_data_exact=0` => resource producer's data not attested; do not invent content SHA.
- `original_name=EXACT` => witnessed by V214 original retail source -> native backing, but still requires appropriate allowlist, material, receiver and asset.
- `native_probe_certified=1` but reject => check precise view / SRV ABI, sampler and resource-snapshot routing, not arbitrary sidecar gain.
- `original_name=AMBIGUOUS` => never choose an alias.

Expected status for new build: CI construction may PASS. Runtime liveness, new origin observation, bridge activation improvement and PTDE pixel comparison remain OPEN until owner runs it.
