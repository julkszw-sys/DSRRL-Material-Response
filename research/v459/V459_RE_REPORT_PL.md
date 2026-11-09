# V459 — native RX34 row30/31 post-V458 closure and minimal runtime preparation

**Project target:** PTDE-visible behavior. DSR host. Frozen V359-RC1 source base SHA `6e852cdc9085dd175159e564e223e0cc8589c535`, development PR lineage V439→V443→V447→V448→V450→V451→V459. One executable ReShade addon; original DSR exe NEVER patched. No LightBank row override, Source1/Source2 change, protected R43/latest_hook_source mutation, global filter or new EXE hook.

## Previously unexploited evidence — exact old runtime CSV

Input `V438_20261107_231505.csv` SHA256 `744d3af90a4f99eff8e7d22d8e78c2c6ced2e09141bda3ea0a23906d3e7d4329`. Run `python research/v459/audit_v438_native_light_pair.py original.csv`.

Native V443 ACTOR_NATIVE kind15: 682 records; **366 exact default-builder certificate status9** (316 alternate status8 not included). V447 native kind16: **366 tickets**. Full exact joining by native producer serial, CPU frame, thread, entity pointer, DrawParam pointer, current/target/alpha and packet A/B/alpha yields **366/366**.

Among 306 RX34 route345 kind17 issued records:
- **294 Source1** records independently exact join to typed default-builder records **294/294**; rows: row30 **66**, row31 **228**.
- **12 Source2 row30** records lack those native-certificate joins and are NOT promoted.
- Six typed native FrpgModelDrawEntity local pointer groups, all sharing FLVER model pointer `0x0E897FC0`: two groups have Light ID `0x51E/0x51E` (row30) and four have `0x51F/0x51F` (row31). **All 366** have native alpha bits zero. Each group has one stable Light pair; no group crosses row30/31. CPU frame buckets may contain both rows but that **does not** prove same rendered pixel or gameplay actor.

| Native entity pointer (session-local) | Light current→target | Samples | Source1 issued RX34 |
|---|---|---:|---:|
| `0x2189D9F0` | `0x51E→0x51E` | 94 | 30 |
| `0x218D2070` | `0x51E→0x51E` | 36 | 36 |
| `0x218D25D0` | `0x51F→0x51F` | 82 | 78 |
| `0x218D2B30` | `0x51F→0x51F` | 84 | 80 |
| `0x218D4B70` | `0x51F→0x51F` | 34 | 34 |
| `0x218D50D0` | `0x51F→0x51F` | 36 | 36 |

**FALSIFIED (scope original capture):** hypothesis that row30/31 alternation in these Source1 RX34 draws is caused by **a native in-entity Light current/target LERP**. These two rows are issued by **different draw entities**; native Light pair values for each are equal, not interwoven.

**Still OPEN:** which entities are PlayerIns, EnemyIns/NPC, equipment or passes, due to missing lifetime/generation map and visible armor match. Shared FLVER and draw context are not actor identity. Source1 equality alone does not certify pixels. Pixel-level comparison against PTDE remains the target.

## New V459 native addon — precise nonmutating trace

Source `renderer-core/tools/v459_light_pair_overlay.py` is installed **after** V451 into one integrated addon by `v459_prepare_build.py`. Reuses exact existing F9 one-shot ≤8s ≤1024 events, RX34 receiver34 route345 rows30/31; avoids new hook/event and any background capture. V451 kind21 uses 17 payload slots, V459 adds seven under previously unused v17..v23:

- v17 bitmask: 1 issued native builder valid, 2 exact `FrpgModelDrawEntity` vtable `exe_base+0x12ECA10`, 4 exact embedded `FrpgDrawParam` vtable `exe_base+0x12E5C08`, 8 DP pointer equals `entity+0x108`, 16 all three Light pair fields readable, 32 duration and elapsed fields readable.
- v18 Light current 16-bit ID; v19 target 16-bit ID; v20 raw float alpha bits; v21 duration float bits; v22 elapsed float bits; v23 actual entity vtable pointer.
- Uses V443 SEH-guarded `read()`; any failed typed identity reports status only. No new eligibility filter on game, no semantic guess or write. Input exact C++ structure is from V458 static RE; builds from source commit, not binary patch.

Native Windows CI run `42710560325`: Linux source-contract PASS, Windows x64 native compile + one-addon PE/marker/SHA PASS. **Binary** `DSRRL_V459_Typed_RX34_Light_Pair_Audit.addon64` size `6,166,016` bytes; SHA256 `5f5139c17bf6df954ef0ed9e957b99bd7e2265b7fef786f9e34ea717fdac5b19`. GitHub Actions artifact ID `13714429473`. ReShade include source pinned `3645e3025d1d98a90e318278858931f034d5d1f6`.

Use `renderer-core/tools/analyze_v459_rx34_typed_light_pair.py` to inspect one runtime CSV, not invent PlayerIns actor identity. V459 captures packet fields and live state jointly, but **does not by itself certify gameplay ownership**.

## Minimal user runtime decision tree (one test, no Quelaag replay)

At fixed known location, keep player in direct sunlight and NPC in adjacent shade, ensure game+addon loaded. Press **F9 once** then rotate camera smoothly for 8 seconds, optionally F10 when visible flicker, then provide new `%TEMP%/DSRRL_V438/V438_*.csv` and `ReShade.log`. Use original known game without another addon in parallel, current addon only. Analyze:
1. On RX34 issued records, if exact typed model Light A/B changes within a stable entity while same resource/material and camera rotates, revisit upstream descriptor writer; compare native issuer with row and receiver. If Source1 mismatches native packet, Source1 cache/routing candidate.
2. If Light pair stays stable per entity but RX34 source row flips within the same **certified generation**, test caching/receiver routing, not native LERP. Generation certificate remains absent, so do not assert same player.
3. If distinct native entities carry distinct fixed rows as before, NEXT must certify gameplay owner and GPU pixel coverage (or controlled NPC presence A/B) before applying a bridge. Source1 294/294 prior success argues against its row selector being the immediate cause, but does not settle a shader/CB compositing error.
4. If typed validation fails, this is **fail-open diagnostic invalid**, not pixel failure or permission for a forced row.

## Status gates

**OFFLINE EVIDENCE:** 366/366 and 294/294 runtime-source replay PASS (original CSV only). **CONSTRUCTION:** V459 source and one addon CI PASS. **COMPATIBILITY:** compile/format only. **RUNTIME LIVENESS V459:** NOT TESTED IN GAME. **ACTOR JOIN:** OPEN. **RX34 GPU/PS/CB PIXEL:** OPEN. **PTDE-visible PIXEL PASS:** OPEN. Reuse of known native Light current=target facts is intentional as a *same issued draw audit*, not another generic LightBank collection. No new canonical PTDE bridge proposed.

Stop condition: do not produce a correction patch until gameplay owner generation and actual consumer/pixel association is certified.