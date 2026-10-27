# V407 — native FrpgDrawContext lifetime and Source1 address-only identity

**Basis:** unmodified DarkSoulsRemastered.exe SHA256 `a45aaa36dd2f6cc151670a639ea5547043cf38ea79ff4178b963c6ed71f98d7b`; V398 sampled GPU-issued handoff (45 rows); Source1 code through V406. Static/source RE only — runtime owner reuse, CB join and pixels OPEN.

## Verified native lifecycle

RTTI `NS_FRPG::FrpgDrawContext` vtable RVA `0x12E6B60`, COL `0x163AB48`; slot 1 at RVA `0x12E6B68` points to **deleting destructor** `0x1EC480`. Factory `0x1EC95F` allocates `0x2500` bytes and calls constructor `0x1EA460` at `0x1EC9CD`. Constructor installs same vtable, stores LightBank wrapper `+0x2318` and manager `+0x2320`.

Deleting destructor RVA `0x1EC480`: vtable install `0x1EC48A/0x1EC493`; cleanup call `0x1EC499 → 0x11B7F90`; tests delete-bit flag at `0x1EC49E`; when set, calls native sized deallocation `0x1EC4AB → 0x10E3F6C` with size `0x2500` set at `0x1EC4A3`. This supplies verified native constructor/destructor hooks as diagnostic boundaries; real invocation/lifetime/reuse OPEN.

## Source cache gap

Under `DSRRL_V398_ACTOR_TRANSACTIONAL_SOURCE`, `pmetal_producer_state.cpp` `model_key()` returns raw `model_owner_instance` pointer. `same_model()` tests owner pointer equality, not per-instance lifetime generation. Exact open-addressed cache of 4096 slots holds producer snapshot by pointer and global epoch. `pmetal_producer_state_clear()` clears only selector TLS state. In the inspected pipeline no `FrpgDrawContext` deletion callback invalidates its pointer key. `g_selector_epoch` is a global lifecycle, not a native context generation.

**Conditional ABA:** if FrpgDrawContext A at P is deleted and allocator reuses P for B without global epoch advance, the old source record cannot be distinguished by pointer+epoch alone. Selector transaction invalidation may prevent particular runtime sequences; address reuse and stale draws are **HYPOTHESIS/OPEN**, never presented as observed flicker.

## V398 native GPU sample census

45 `indexed_issued` sampled draws contain 3 logged owner pointers. `0x219A0D00`: 5 issued / 5 unique FLVER / 5 Source1 / 0 Source2. `0x219A2800`: 23 issued / 8 FLVER / 6 Source1 / 17 Source2. `0x219A4D80`: 17 issued / 3 FLVER / 8 Source1 / 9 Source2. One owner pointer across multiple **FLVER assets** is confirmed; a gameplay actor can contain multiple FLVER assets, so this is NOT proof of cross-actor sharing. Sample frequencies are not representative of global runtime. Owner-to-native-packer-sequence-to-RX34 GPU draw identity remains unverified.

## Recommended narrow next test

Add read-only native context allocation/deletion generation tracking `(context_pointer, lifetime_generation)` with ABI-certified constructor and deleting destructor, joined to selector owner, exact wrapper, manager, producer sequence, receiver-specific indexed draw and D3D CB. Record bounded transitions. This must not alter `latest_hook_source()` protected R43 behavior, block row25/row0, fake per-material ownership, or patch the game executable on disk.

**Status:** static lifetime RE CONFIRMED, sampled multi-FLVER-by-context CONFIRMED, addon cache lifetime-generation missing CONFIRMED, ABA/Flicker/GPU CB/pixel OPEN. Local archived reproducibility kit: `DSRRL_V407_CONTEXT_LIFETIME_RE_AUDIT.zip`; test requires owner-supplied EXE and V398 archive.
