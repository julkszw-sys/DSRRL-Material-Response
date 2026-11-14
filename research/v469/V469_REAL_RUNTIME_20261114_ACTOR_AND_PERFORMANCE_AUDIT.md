# V469 runtime — user-provided 2026-11-14 ReShade log

## Evidence identity
Original uploaded `ReShade(20261114-113126).log`: 2,831 lines, raw file 1,819,262 bytes, SHA256 `a39ee3342813244e097bf5ba7fa402d4aa879689fe7b2b57f9a6fb5031d5871c`. Log window 12:29:21.783–12:31:04.226 local. Full local private paths are not needed for reproduction.

## Construction/runtime breakdown
- V469 single addon loaded, line 46.
- `[DSRRL V468] CHRINS_OWNER_HOOK_DISABLED_PERF original_game_code=1 no_native_detour=1` line 51, therefore native entry=0 in later V467 summary is intentional and CANNOT establish the original native method was unused.
- V465 auto capture starts 12:29:44.075 and ends 12:30:09.085 (~25 s).
- V466: six exact full receiver handles, 296 accepted issued draws, 322 trigger hits, 0 owner observations, 0 conflicts, 6 unknown; two ROW30 handles (81000f10=62 draws and 81000d8d=32 draws) and four ROW31 handles (81000d8c=66, 81000d8b=66, 81000d8a=36, 81000d89=34 draws).
- V467 independent exact typed `WorldChrManImp → PlayerIns → ChrModel → model+0x10` read: `world_queries=296 world_exact=296 player_exact=296 model_exact=296 player_handle_read=296 world_full_match=0 world_full_mismatch=296`. This **falsifies direct equality of the root PlayerIns model handle and these six RX34 issued receiver handles in this run**, but does NOT falsify player-owned equipment/child renderable identity and does NOT prove NPC or pixel coverage. Do not auto-authorize GPU actor joins from this.
- V469 has no dedicated runtime marker; identity is exact addon filename and V468 disabled-hook marker; generic build ID embedded still states V398/V230 source ancestry, not a V469 release.

## Diagnostic overhead evidence
Comparison with user-supplied V466 log from 2026-11-13, which covered a somewhat different session (not controlled same-camera FPS test):
- V466 4,293 lines vs V469 2,831 lines; peak single second 1,508 vs 675 lines.
- V415 MODEL SELECTOR: 525→37. V415 MODEL SOURCE JOIN: 516→16. V416 FGMODEL PARENT: 269→30. V417 FG REGISTRY PROVENANCE: 269→30. First burst reduction is confirmed.
- V411 RX34 OWNER SEQ DRAW: 576→576; V418 RX34 MODEL ISSUED JOIN: 576→576; V409 RX34 GPU TRANSITION: 320→320. 121 of V411 and 121 of V418 log messages occurred after the V465 capture ended.
- V417 count reaches 180,224 in V469 (versus V466 106,496), despite only 30 V417 text messages. Source `flver_engine_hooks.cpp` shows, when V417 diagnostic active, exact `range_ok` memory validations, `std::memcpy` from upstream native FgModel and `v415::record_registry_observation`, plus relaxed atomics. Reduced logging does NOT remove work; measured CPU cost remains OPEN.
- One-time GPU preload: 1,822 DDS, 559,110,352 disk bytes; is startup work, not confirmed ongoing per-frame FPS cause.
- No frame-time/FPS measurements in this log, so performance regression attribution remains OPEN.

## Narrowest valid next investigation
Separate the **RX34 issued draw observer** from **general V415/V416/V417 model-selector instrumentation**; keep typed source identity and all renderer operators. Before any compile-time pruning, census which parts of V415/417 state V418 consumes and preserve exact receiver/material identity. Prior V470 blanket INFO log deletion failed mandatory source checks, closed PR548; do not resurrect indiscriminately.

Status: addon loaded PASS; RX34 consumer hit PASS; world/player/model class checks PASS; direct root handle equality REJECTED in this capture; equipment/submodel owner mapping OPEN; shader/material/pixel equivalence OPEN; FPS/frametime OPEN. User is NOT required to perform new runtime tests to advance static RE.
