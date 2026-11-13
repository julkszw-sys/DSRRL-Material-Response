# V466 — RX34 owner pre-window evidence (single integrated ReShade addon)

## Target and lineage

- Target: attribute actual RX34 row30/row31 native draws to typed `PlayerIns/EnemyIns → ChrModel → FrpgModel+0x10`, without using shared FLVER identity as an owner proxy.
- ReShade lineage: **V359-RC1 frozen active development base → V439/V443/V447/V448/V450/V451/V459 → V465 → V466**.
- Source branch: `agent/v466-rx34-prewindow-owner-evidence-one-addon-20261113`; PR #544 targeted to the V465 diagnostic branch, not `main`.
- No LightBank, R43, Source1, Source2, shader, CB, material, pixel or original EXE disk mutation. No new native hook is introduced by V466.

## Independently checked original DSR EXE

SHA-256 `a45aaa36dd2f6cc151670a639ea5547043cf38ea79ff4178b963c6ed71f98d7b`, PE32+ image base `0x140000000`.

Direct original EXE pointer reads establish the following MSVC vtable entries:

| Class | vtable RVA | slot +0x120 absolute VA | target RVA |
| --- | --- | --- | --- |
| PlayerIns | 0x13251F0 | 0x140321CE0 | 0x321CE0 |
| EnemyIns | 0x1322E68 | 0x140321CE0 | 0x321CE0 |
| ChrIns | 0x131E008 | 0x140321CE0 | 0x321CE0 |

At `0x321CE0`, exactly 13 relocation-free prologue bytes are `48 89 5C 24 18 57 48 81 EC C0 00 00 00`; at `0x321D05`, `48 8B 49 60` reads `owner+0x60`; at `0x321D12`, `48 83 7F 68 00` checks `owner+0x68`. The entry method is a plausible read-only observation point shared by these three vtable families. **The direct native linkage does not prove runtime call frequency, gameplay owner identity, or RX34 GPU pixel ownership.**

Reproduction: `python research/v466/verify_v466_chrins_bridge.py DarkSoulsRemastered.exe --mutations`.

## Why V466 is needed

V465 captures typed owner samples only while its 25-second auto-capture state is `active`. If the native ChrIns method was invoked before the RX34 trigger and not again during the active interval, a draw can appear without a typed owner even though a previous observation was possible.

V466 keeps bounded owner samples before the trigger using the existing hook and the same 256-entry full-handle registry. Evidence is now partitioned:

- `CPU_JOIN`: exact typed owner, identical 32-bit model/receiver handle, nonconflicting owner/entity, and **an owner hit during the active capture window**.
- `PRETRIGGER_ONLY`: matching typed owner observed earlier, but no owner hit in the active window. This is an association candidate, not proven lifetime continuity or an authoritative owner join.
- `UNKNOWN`: ambiguous type, missing join, owner/entity collision or other insufficient evidence.

An existing entity/handle observed issuing both row30 and row31 is explicitly reported as `row_scope=BOTH_ROWS`. This is evidence about native draw routing across observed events, **not proof that both affect the same GPU pixel or that one row is incorrect**.

The capture remains automatic and logs aggregate results once into `ReShade.log`; no F9/F10, no mandatory CSV. The same `.addon64` is reproduced by the V466 CI build.

## Expected validation levels

| Level | Status | Notes |
| --- | --- | --- |
| Static source / EXE operator observations | CONFIRMED within cited addresses | EXE hash and vtable/prologue witness |
| Construction | CI-dependent | Reproducible one native addon, no build claim beyond CI |
| Runtime liveness | OPEN | Need actual ReShade.log |
| Owner/receiver association | OPEN | Needs real simultaneous observation |
| Bridge activation | NOT APPLICABLE | No lighting operator changed |
| Pixel / PTDE-visible behavior | OPEN | No visual fix is claimed |

**Remaining risk:** pre-trigger observation expands diagnostic reads before auto capture. Lock drops, overflow, and performance effects are not proven harmless; runtime telemetry and owner playback are needed. Do not promote `PRETRIGGER_ONLY` or a raw 32-bit-handle match to final gameplay ownership/pixel equivalence.
