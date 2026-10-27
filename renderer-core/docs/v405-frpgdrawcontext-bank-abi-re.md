# V405 — FrpgDrawContext LightBank semantic join RE

**Binary:** original unmodified DarkSoulsRemastered.exe SHA256 `a45aaa36dd2f6cc151670a639ea5547043cf38ea79ff4178b963c6ed71f98d7b`; base `0x140000000`. All addresses RVA. Independent local validator in `DSRRL_V405_CONTEXT_CACHE_RE_AUDIT.zip` passes 53 exact instruction checks plus RTTI, vtable and factory tests. No runtime/pixel claim.

## RTTI correction and exact source layout

At `0x1EA48C/0x1EA493`, native initializer `0x1EA460` installs vtable RVA `0x12E6B60`. MSVC COL at `0x163AB48` references TypeDescriptor `0x1AF8348`, identifying `NS_FRPG::FrpgDrawContext` (`.?AVFrpgDrawContext@NS_FRPG@@`). **Not** `FrpgDrawEntity`. Do not assume the V398 variable called `model_owner_instance` is already proven to identify a gameplay actor instance.

The same `RSI` context object receives:
- `0x1EA4C4`: `[RSI+0x2318]` ← native LightBank wrapper.
- `0x1EA4CF`: `[RSI+0x2320]` ← source-derived manager pointer (`[RAX+0x28]`).
- `0x1EA4EE`: `[RSI+0x2338]` ← initialized fallback.

The earlier V401 paths dereference `owner+0x2318`. The ShaderContext helper `0x1EABF0` dereferences `destination+0x2320` and `+0x2338`. This is a verified common native **ABI/layout**, but not yet a verified identical live pointer or actor-qualified RX34 draw.

## Distinguish LightBank ID from LightBank pointer

`0x1EABF0`: `mov r8d,6` at `0x1EABF9`; `mov rcx,[rcx+0x2320]` at `0x1EABFF`; calls `0x569B70` at `0x1EAC06`; if zero, falls back to `[rbx+0x2338]` at `0x1EAC0F`.

`0x569B70` bounds area 0..11/type 0..12, uses `area*0x1B0` to find `[manager+0x20+area*0x1B0]`, and reads **32-bit ID/value** from `[table+type*0x10]`. Distinct native pointer getter `0x569B40` reads **source pointer** from `[table+type*0x10+8]`. Do not join actors via a shared bank pointer or treat these two fields as equivalent.

## ShaderContext partial update hazard

`NS_FRPG::ShaderContext_FrpgFxNormalEntity` constructor sets `+0x60=null`, `+0x68=0`, `+0x6C=-2`, `+0x70=-2` (RVAs `0x4FF604..0x4FF612`). Method22 `0x5000E0` calls method21 `0x4FFF10` through slot `+0xA8`, forwarding the same RDX destination. Both compare cached source ID `+0x68`; method21 may skip writes to destination `+0x1240..0x1260` when index `+0x6C` agrees (`0x4FFFDC/0x4FFFDF`); method22 similarly uses `+0x70` for `+0x1320/+0x1330` (`0x5001C4/0x5001C7`). The examined source-ID update branch does not also invalidate both index caches. Correctness depends on destination lifetime and other state transitions; not evidence of flicker by itself.

Both methods call `0x1EABF0` with **RDX destination** as helper RCX, making the `FrpgDrawContext`-like bank ABI an explicit input. **Live pointer identity, receiver family, D3D11 CB upload and PTDE pixel equivalence remain OPEN.**

## Falsifiable next steps

Verify exact native selector context vtable/lifetime, relation to gameplay actor, both bank-entry fields (type6 +0 and +8), and immutable `context → wrapper → producer_sequence → RX34 indexed draw` with frame/command list; then census CB shader consumer. Preserve protected R43 `latest_hook_source`, no row veto, no arbitrary gains, no EXE disk patch. 

Statuses: static PE RE **CONFIRMED**; addon construction **NOT ATTEMPTED**; compatibility/runtime/activation/pixel **OPEN**.
