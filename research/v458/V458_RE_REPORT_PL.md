# V458 — FrpgModelDrawEntity → FrpgDrawParam → Light dual-ID consumer (static RE)

**Target:** PTDE-visible lighting. **Host:** DSR. Exact unmodified original EXE SHA256 `a45aaa36dd2f6cc151670a639ea5547043cf38ea79ff4178b963c6ed71f98d7b`. No addon, executable, R43, Source1/2, shader, LightBank row or resource edits.

## Verified typed identity and command routing

- RTTI/vtable `0x12ECA10` = `FrpgModelDrawEntity`, slot **+0x20 → 0x223380**, native opcode dispatcher; **+0x50 → 0x220000**, relevant builder wrapper, whose `0x22008C` calls `0x2200D0`.
- `FrpgModelDrawEntity` constructor `0x21F900`: `0x21F9BE` passes `entity+0x108` to `0x1D7490` at call `0x21F9C5`. Typed `FrpgDrawParam` RTTI/vtable `0x12E5C08` is installed there.
- V457 opcode `0x804C` dispatcher `0x223380` table entry `0x2235E0` selects `0x223466`; `0x22346C → 0x222B60` reads float and Light/Fog/Scattering packed IDs, then `0x222BBB → 0x1D7580` with `RCX=entity+0x108`.
- The embedded typed `FrpgDrawParam` has three **16-byte temporal transition records**, each with current and target 16-bit IDs, float weight, duration and elapsed time. Light begins `entity+0x110` (target `+0x112`, weight `+0x114`), Fog at `+0x120` and Scattering at `+0x130`. `0x1D7580` invokes `0x1D7820` three times. `0x222D30` advances the state using `entity+0x108` and `0x1D7530`.

## Exact native draw-builder reads and consumer

In `0x2200D0`, `RDI=FrpgModelDrawEntity*`. `0x22024C` and `0x22025B` select `entity+0x110`. Calls `0x220256→0xC04A0` and `0x22026E→0xC0550` read the **current** and **target** Light words. `0x220273` reads **weight float** at `entity+0x114`. These values enter the temporary draw-state packet at `+0x1C/+0x1E/+0x20`.

The builder also routes `entity+0x108` via `0x220614` and `0x220639→0x1C0BE0→0x1C6900` in its matching branch. This is the concrete Light-response consumer:
- At `0x1C693F/6951`, get current/target Light IDs from `FrpgDrawParam+0x8`, compare at `0x1C695F`.
- **Equal:** resource lookup `0x569B40` with resource-category argument `R8D=5` (`0x1C697B/6986`) and a fallback branch.
- **Different:** reads transition weight `FrpgDrawParam+0xC` (`0x1C6ADA`), resolves **two category-5 Light resources** (`0x1C6AF7/6B00` and `0x1C6B32/6B3D`), invokes dual-input response `0x5637E0` (`0x1C6B75`), and processes vectors including `0x1C6B9E`. Outputs are staged under a CPU draw-state object at `+0x1240..+0x12B0`.

This establishes **local native Light current/target resolution and a two-resource blending path**. Category 5 here represents a **Light resource lookup family**, not a gameplay actor ID. Category 1 from other writer families is not assigned a global meaning by this result.

## Scope and falsifiers

**CONFIRMED STATIC:** typed entity receiver, embedded `FrpgDrawParam`, command dispatch, three transition records, first Light draw-builder reads, two-resource Light consumer and relevant CPU staging.

**OPEN:** exact `FrpgModel/registration handle → concrete receiver generation`; whether the observed RX34 row30/31 are current/target IDs of **one** entity; mapping to PlayerIns/EnemyIns; receiver-specific shader/constant-buffer GPU consumption; actual same-pixel overlap, PTDE-visible pixels and residual. The six RX34 entities in V438 (2 row30/4 row31), with Source1 294/294 source→issued matches, prove native packet consistency, **not gameplay owner**. A shaded nearby NPC remains a hypothesis, not a confirmed wrong-row donor.

**Next narrowly scoped experiment:** reuse one integrated V451 addon, manual F9 8s ≤1024 events, with typed `FrpgModelDrawEntity* + generation`, exact `FrpgDrawParam.Light current/target/weight` sampled only for six RX34 entities, Light packed key and actual row/source/packet/material/receiver. Add owner-level PlayerIns/EnemyIns generation only after typed identity validated. Fail open to DSR if identity mismatch. Do not force row30 or globally alter LightBank.

## Reproduction

Repo source `research/v458/verify_v458_instance_light_bridge.py` imports exact existing `research/v457/verify_v457_selector_consumer.py` in same branch. The exported local handoff also contains a self-contained verifier. Read-only SHA-locked original EXE required. **116 opcode preimages, 25 direct E8 edges, 2 RTTI types, 3 vtable slots, 125/125 mutation controls PASS.** Construction and static only; runtime liveness/activation/pixel are not promoted.