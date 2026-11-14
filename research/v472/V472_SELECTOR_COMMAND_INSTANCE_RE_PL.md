# V472 — LightBank selector: exact per-instance 0x804C transport and RX34 class-2 falsifier

**Scope:** original SHA-locked DSR retail executable `a45aaa36dd2f6cc151670a639ea5547043cf38ea79ff4178b963c6ed71f98d7b`, V469 user log `a39ee3342813244e097bf5ba7fa402d4aa879689fe7b2b57f9a6fb5031d5871c`, RX34 route345 Source1, P_Metal. The PTDE-visible image is the target. **This is a source-level selector RE result, NOT a new addon or a claimed pixel fix.**

## 1. Separate actual selectors

**Not the LightBank input:** `0x22BA20` selects a shader-set handle. It is not proven to be the physical LightBank row/actor selector and must not be used as the bridge's source.

**Actual native LightBank command writer:**

1. `ChrModel` vtable `RVA 0x12EAF30+0x20` invokes override `0x20B4E0`. It calls FrpgModel base update `0x21C970` at `0x20B546`, retaining the model instance.
2. `0x21C970` tests the model's dirty bits `[model+0x84] & 0xE0`; when set, `0x21F600 / 0x21F5C0 / 0x21F640` encode selector channels. It reads the **same model's** complete 32-bit registration handle `[model+0x10]` at `0x21CAA1` and invokes `0x222AC0` at `0x21CAC7`.
3. `0x222AC0` emits opcode `0x804C` through `0x11B3710`, with instance-handle destination and packed selector payload. The unmodified DSR dispatcher `0x223380` recognizes `opcode-0x8046`, jump-table case 6 (`0x223466`), and calls `0x222B60` (`0x22346C`).
4. `0x222B60` decodes into the **addressed FrpgModelDrawEntity's** embedded `FrpgDrawParam` at `[entity+0x108]`, calling the native three-lane interpolation/retarget helper `0x1D7580` at `0x222BBB`.
5. Receiver constructor `0x21F900` initializes entity+0x108 via `0x1D7490`; `0x2200D0` native builder reads the same embedded DrawParam and emits LightBank via verified `0x220632/0x220639` and adjacent native emitters, before the typed model draw.

**First semantic cut for a correct bridge:** exact `FrpgModel instance + full registration handle + 0x804C command` ↔ exact `FrpgModelDrawEntity instance + full handle + embedded DrawParam + issued RX34`. No FLVER-only, MTD-only, global context or nearest-last-packer shortcut can establish that relation.

## 2. Root model and renderable are not automatically the same registration

`PlayerIns/EnemyIns+0x60` points to typed `ChrModel` with vtable RVA `0x12EAF30`, while RX34 `FrpgModelDrawEntity` has vtable `0x12ECA10` and a **separate constructor and handle registration**. `0x223AC0` forwards its input `R8D` to receiver ctor `0x21F900`, base `0x1F2DB0`, and `0x11B8910` writes `receiver+0x10`. `ChrModel+0x10` is initialized separately in its own constructor. Exact vtable identity and the equal field offset do not mean the same actor-instance handle. The earlier 0/296 root-PlayerIns versus RX34 handles is therefore an expected *possible* outcome, not evidence that the RX34 entity belongs to an NPC.

`FrpgModelDrawEntity+0x40` is copied from the fourth ctor parameter's `+0x20` in `0x21F900`. The common `0x223AC0` factory relays its own `this` as that fourth argument; therefore this resource path is not a certified gameplay-owner identity. Historical six RX34 receiver entities sharing a model/FLVER pointer likewise cannot provide actor ownership.

## 3. V418 class-1-only gate excludes observed class-2 RX34

Source `renderer-core/include/dsrrl/runtime/v415_model_draw_identity.hpp`: V415 stamp is selector-thread local. `record_verified_fgparent` is deliberately allowed only for `class_kind==1` (FgFlverModel). V417 registry observation depends on `fg_parent_reciprocal`. The `renderer-core/src/runtime/pmetal_env_source_runtime.cpp` V418 journal and integrated V419 pre-draw witness require `fg_reciprocal && registry_readable && registry_allocated` for the strongest join.

Authenticated owner V469 log has **317** V418 route345 messages: Source1 **299**, Source2 **18**. **All 299 Source1** carry exact typed `class=2 FlverModel`, `model_verified=1`, but `parent_reciprocal=0`, `registry_readable=0`; hence `exact_renderable_join=0`. Row30 Source1 **68**, row31 Source1 **66** in the logged subset have the **same** source-stage FlverModel pointer `0x0FA3F760`. This is a hard falsifier of treating the V418 Fg-parent condition as a universal LightBank actor identity path.

**Crucial limitation:** V418's failed join is an **observer/gate for optional diagnostic attribution**, not evidence that the LightBank shader operator incorrectly ran. It must not be silently widened to `class=2` without independently proven full-handle command-instance ownership.

## 4. What to fix next — exact algorithm, not arbitrary row mapping

**Receiver-first command instance bridge**:

- Producer: original FrpgModel (including equipment submodels), exact vtable family and `+0x10` full handle, opcode `0x804C` payload and current/target selector channels.
- Transport: native command instance lookup; verify same exact full registration handle and lifetime with the receiving FrpgModelDrawEntity.
- Consumer: decoder `0x222B60`, embedded DP `+0x108`, native issue `0x2200D0`, then exact RX34 shader / material / resource.
- Owner: prove which *specific FrpgModel instance* belongs to PlayerIns, EnemyIns or equipment child, independent of generic shared FlverModel and shared FrpgDrawContext. No owner from class2, root handle mismatch, FLVER SHA, or bank row alone.
- Target: compare PTDE-visible operator behavior for that semantic actor/material pair; choose narrow PARAM/shader/CB/resource/material carrier. Do not remap DSR row30→31 or freeze a row without this evidence.
- Fail-open: unmatched command, stale handle, class mismatch, wrong shader/material/resource, or ambiguous actor stays **stock DSR**, preserving independent operator islands and protected R43 `latest_hook_source`.

**Status:** exact native command mechanism **CONFIRMED STATIC**; class-2 rejection of V418 universal gate **CONFIRMED RUNTIME**; actor→specific submodel/registration, PTDE row target and pixel **OPEN**; no new canonical selector patch yet.

## 5. Reproduction

```sh
python research/v472/verify_v472_selector_command.py 'DarkSoulsRemastered.exe' --mutations
python research/v472/verify_v472_selector_v469_log.py 'ReShade(20261114-113126).log'
```

Expected EXE: 28 opcode witnesses, 9 exact E8 edges, 2 vtable slots, **28/28** mutated instructions independently detected. Expected log: 317 V418 RX34 entries, Source1 299 class2, 0 Fg-parent, 0 registry, 0 exact actor join. Both scripts SHA-lock inputs and fail closed on unrelated inputs.

No runtime hooks, native EXE-on-disk patches, shader/LightBank/DrawParam modifications, new addon, or performance work in V472. Existing V469 one-addon artifact and frozen V359-RC1 release remain unchanged.
