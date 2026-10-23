# V230 Source1 requirements audit — 2026-10-23

Scope: *Source1 exact P_Metal RX34/slot2 Quelaag-REMO cutscene*, not general armor and not another producer optimization. Source versions: `pmetal_env_source_runtime.cpp`, `pmetal_producer_state.cpp`, `pmetal_envspec_draw_runtime.cpp`, `material_resource_draw_runtime.cpp`, `pixel_srv_shadow.cpp`, `spec_rgb_bridge.cpp`, `mtd_semantic_census.cpp` on PR #515.

## Primary runtime evidence — user-supplied 2026-10-23 16:42 log

Log: `ReShade(20261023-164237).log` SHA256 `b7013459c85860c7324704ac9cb2372091c1c1043487cc51564bbb19663707a4`.
- `SOURCE1 HIT`: 41 exact producer/consumer source successes, `route345 slot2 flver8=3a82e4f6 mtd8=ece70f36 owner=1 actual=1`. Therefore the selector source *can* match, contrary to extrapolation from non-Quelaag log.
- `RX34 FRONTIER`: 26 observed preparations all reach phase7 (probe ready) but fail before SpecRGB.
- `RESOURCE CUT V2 kind=source1`: 20/20 with `stockmask=7 hashmask=0 sidemask=0`, `h0=h1=h2=0`. Three bound native stock SRV handles exist; their logical identities are NOT in the addon cache. This does not prove exact PTDE sidecars are absent from disk.
- `RX34 SRV CUT`: 20/20 have `shadow_material=0 shadow_env=0 fallback_context=1`. Native context interrogation supplies the views, but no independent push-descriptor shadow witness is available. It is a fallback draw-state path, not proof of the expected resource identity.
- `DUAL FRONTIER kind=source1 phase=spec_decision`: 18/18 `sem_spec=1 sem_diff=1 sem_norm=1`, `spec_consumer=1 feature_spec=1 full_mr=1`, but `spec_reason=4 ptde_companion_not_verified; spec_action=0; prepared_spec=0`. Therefore **MTD semantic gate passes**.
- Historical 20261023-160608 and 162048 runs reproduce all three masks and SpecRGB reason. The subsequent 172545 owner log reports zero pixel improvement but **does not contain RX34 Source1 target draws**, so its 967 publications / 0 exact read MUST NOT be attributed to target Source1 regression.

## Producer -> transport -> consumer -> pixel requirement audit

| Requirement / implementation | Evidence | Verdict |
|---|---|---|
| Exact FLVER SHA256 + material slot + raw MTD SHA + receiver RX34 for Source1 | `SOURCE1 HIT`, `RX34 FRONTIER`, `DUAL FRONTIER`; attested tuple | **KEEP / CONFIRMED PASS** for scene |
| Selector return-RVA, parent return, descriptor-owner, selector endpoint & LightBank decoder checks | 41 producer exact hits and phase7 in this exact RX34 case | **KEEP**; not current blocker |
| `pmetal_producer_state_latest()` same material key and epoch | 41 exact hits in this same scene; 0 exact from unrelated non-Source1 scene does not falsify it | **KEEP**; not global Source1 cause |
| EnvSpec probe resource + material/receiver island | phase7 reached 26 times | **KEEP / CONFIRMED PASS** |
| MTD semantic `spec_rgb == use`, receiver and t10 consumer | 18/18 Source1 `sem_spec=1`, `spec_consumer=1` | **KEEP / CONFIRMED PASS** |
| Native stock t0/t1/t2 SRVs physically bound | 20/20 stockmask7 | **CONFIRMED PRESENCE**; their logical identity remains unknown |
| A `snapshot_raw` name must be current when `on_init_resource_view` runs before `g_cache` learns this view | `on_init_resource_view` early return on missing TLS name; `inspect_many` only checks cache keyed by native SRV pointer; 20/20 Source1 hashmask0 | **BLOCKING CARRIER ASSUMPTION / HIGH CONFIDENCE**. Exact identity IS required, but this particular loader-name/TLS route should NOT be mandatory if exact attested resource identity is available by a narrower owner/asset mapping. Need verify whether Source1 views are unnamed at create-time, aliases, or missing registration before patch |
| `spec_equipment_allowed(h1)` and `generated::spec_equipment_name_hash_allowed_v12` for dedicated cutscene Source1 | `material_resource_draw_runtime` reuses equipment-only SpecRGB sidecar admission; P_Metal is a valid non-armor Source1 material. Actual h1 currently zero, so downstream allowlist has NOT been tested | **SCOPE MISMATCH RISK / HIGH CONFIDENCE**, NOT confirmed fail. Separate certified Source1 texture map from equipment cohort; no blanket allow |
| Exact PTDE RGB companion loaded and DSR stock t1 preserved | `h1=0`, `sidemask=0`, `spec_reason=4`; PTDE t10 never bound | **KEEP semantic requirement / CURRENT VERIFIED FAILURE**. Do not fake `exact_companion=true` or bind unrelated DDS |
| `stock_t1_preserved=true` currently hardcoded in `spec_rgb_context` | Source code, no observed stock-t1 restoration proof | **WEAK VERIFICATION**, not immediate block. Assert in downstream draw transaction tests |
| `shadow_material_ready` at prepare | 20/20 false, code falls back to `PSGetShaderResources` on `cmd_list->get_native()` | **NOT a mandatory gate** (already fallback); compare bound view identity before treating fallback as authoritative |
| Sidecar eligibility tested again by `spec_equipment_allowed(h1)` after creation | Source code class membership is already part of companion-sidecar admission | **REDUNDANT CHECK IN CURRENT PATH**, but removing it cannot recover `h1=0` and must not broaden other materials |

## Why the previous changes did not fix pixels

Widening admission for **named** stock views still requires `texture_identity_transport::snapshot_raw` to succeed. It cannot establish the logical identity of views that do not have a valid name in that callback, nor a view that is an unrecorded alias of the named view. The negative owner test is expected to remain visually unchanged without new *verified* resource mappings. Do not equate cache liveness, successful source HIT, or resource preparation with pixel improvement.

## Narrow remediation, not a global whitelist rollback

1. For **this exact RX34/route345/FLVER SHA + slot2/raw MTD tuple**, inspect actual t0/t1/t2 native SRV pointers, underlying `ID3D11Resource` identities and resource-view creation/history; compare at the true draw-bound consumer. Log missing-name/no-view-record/alias/ambiguous conditions separately and count each.
2. Derive t1 logical texture identity from certified Source1 FLVER+material-slot/asset binder or a verified owner-to-native-resource mapping, not from a guessed filename, similar hash or available DDS. Compare to stock resource identity; exact PTDE specular data must be homologous.
3. Permit identity lookup through the attested resource mapping only when source, native format, receiver, material and lifetime all match; otherwise **fail-open**. Do not weaken the global equipment path.
4. Retain independent PTDE t10 SpecRGB consumer, exact sidecar, owner and D3D state transaction. Only after `h1`, companion and requested t10 are proven may phase8 be promoted. Phase9, actual draw and PTDE pixels must be validated separately.

**Status:** construction/runtime source and receiver HIT CONFIRMED; first blocking frontier `Source1 logical t0/t1/t2 association` CONFIRMED; origin (unnamed view vs alias vs missed callback vs missing PTDE sidecar) OPEN; downstream Source1 allowlist mismatch HYPOTHESIS; PTDE pixel FAIL per owner in unrelated latest diagnostic run / Source1 pixel OPEN. No runtime renderer changes or active V359-RC1 promotion authorized by this audit.
