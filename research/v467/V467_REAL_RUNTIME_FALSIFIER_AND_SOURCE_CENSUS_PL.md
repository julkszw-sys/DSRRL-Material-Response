# V467 — RX34 typed WorldChrMan PlayerIns source census, driven by V466 real runtime

## Runtime falsifier

Owner-supplied `ReShade(20261113-202623).log`, 4293 lines. Native V466 ReShade addon loaded successfully; `[DSRRL V465] CHRINS_OWNER_HOOK_ACTIVE rva=321CE0 exact_exe=1` at 21:24:01.014. Automatic RX34 capture 21:24:40.488–21:25:05.812 (25.3 seconds).

Recorded `[DSRRL V466] SUMMARY handles=6 live_joined=0 prior_only=0 unknown=6 conflicts=0 owner_samples=0 pre_owner_samples=0 live_owner_samples=0 rx34_draw_samples=276 rx34_trigger_hits=302 invalid_owner=0 invalid_draw=6 lock_drops=0 overflow=0`.

Six exact receiver identities:

| Issued full handle | Row | Draw hits | FrpgModelDrawEntity |
| --- | ---: | ---: | --- |
| 810009fd | 31 | 80 | 0x21914790 |
| 81000f11 | 31 | 78 | 0x21914cf0 |
| 8100086d | 30 | 12 | 0x218e9d10 |
| 81000a38 | 31 | 40 | 0x21937130 |
| 81000f10 | 31 | 38 | 0x21937690 |
| 81000f12 | 30 | 28 | 0x21914230 |

The **RX34 issued-event collection is live and returns real full handles**. However, none of these was attributable to a typed `PlayerIns` / `EnemyIns` in V466: `owner_hits=0` for all six. `HOOK_ACTIVE` establishes a patched entry, not that the hooked native method executed. Current owner classification is therefore OPEN. Do **not** infer that row30 is player, row31 is NPC, or vice versa.

## V467 action

Continue frozen V359-RC1 descendant `V439 → V443 → V447 → V448 → V450 → V451 → V459 → V465 → V466 → V467`, exactly one integrated ReShade addon.

Two independent read-only diagnostics:

1. **ChrIns source liveness.** Increment counters for actual entries to the existing RVA `0x321CE0` detour, nonnull RCX owners and typed vtable classification `PlayerIns` / `EnemyIns` / `ChrIns`. Log once on auto-capture finish as `[DSRRL V467] OWNER_SOURCE_DIAG`. If entry count is zero, the existing hook may be valid but is not an active source for the captured scene.
2. **Same-draw typed world player candidate.** At the existing certified RX34 route345 **Source1** issued-draw event, read `WorldChrManImp` singleton global RVA `0x1C77E50` and require exact vtable RVA `0x1328210`. Resolve `WorldChrManImp+0x68` actor pointer and require exact `PlayerIns` vtable RVA `0x13251F0`, `PlayerIns+0x60` exact `ChrModel` vtable RVA `0x12EAF30`, then read `FrpgModel+0x10` full 32-bit handle and compare to issued `FrpgModelDrawEntity+0x10`. Record stage counters even on failure. If all match in the same draw, emit `[DSRRL V467] WORLD_PLAYER_HANDLE status=TYPED_PLAYER_CANDIDATE`. This does not set V465 hook `CPU_JOIN`; it does not prove lifetime continuity or pixels.

Out-of-source, wrong vtable, unreadable pointers, wrong RX34 entity or wrong full handle yield no owner assertion. The failure path always leaves stock renderer untouched.

## Required falsification after build

In the same player/NPC sun/shadow case load only the V467 addon, let automatic RX34 capture finish, then inspect `OWNER_SOURCE_DIAG`, `WORLD_PLAYER_HANDLE`, and V466 `ACTOR_HANDLE` / `SUMMARY` records in `ReShade.log`.

If `native_entries=0` while `world_player_exact` or `world_full_match` is positive, V465's method hook was not a useful live owner tap; the independent typed world candidate can still provide an actor routing cut. If `world_exact=0`, revisit the precise global/manager route, not the lighting operator. If `world_player_exact=0` despite `world_exact>0`, resolve the actual +0x68 owner semantics dynamically instead of asserting player ownership.

## Scope and statuses

Static vtable/offset RE: **CONFIRMED** for SHA-locked original DSR EXE. V466 receiver/handle liveness: **RUNTIME CONFIRMED**. V467 source contract and native build: CI dependent. V467 actual native hook calls, world candidate, CPU ownership, RX34 actor, downstream material and PTDE pixel: **OPEN until runtime evidence**.

No new native hook; no Source1/Source2/R43, LightBank, shader, CB, resource, material, DrawParam or EXE-on-disk modification. All owner observations are diagnostic only.
