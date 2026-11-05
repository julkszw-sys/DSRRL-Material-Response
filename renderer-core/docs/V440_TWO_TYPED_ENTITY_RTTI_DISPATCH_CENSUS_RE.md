# V440-R4: two sibling native DSR actor classes and conditional draw dispatch

Original untouched retail DSR SHA256: `a45aaa36dd2f6cc151670a639ea5547043cf38ea79ff4178b963c6ed71f98d7b`.

**Original MSVC RTTI confirmed** (not class guesses):
- vtable RVA `0x12EBFE8` has complete object locator `0x163D2F8`, TypeDescriptor `0x1AF9920`: `FrpgAsmModelDrawEntity`. vtable+0x50 => dispatch RVA `0x2161F0`.
- vtable RVA `0x12ECA10` has COL `0x163D738`, TypeDescriptor `0x1AF9BA0`: `FrpgModelDrawEntity`. vtable+0x50 => dispatch RVA `0x220000`.
- Their separate ClassHierarchyDescriptors list **identical inherited base descriptors**: `FrpgDrawEntity` `0x163B278`, `DLHG::HGDrawEntity` `0x16E1FC0`, `DLHG::HGEntity` `0x16E1A60`. These are sibling classes, **not** one actor class with two aliases. Exact original RTTI PMD and base array checked. Both have an embedded DrawParam source at actor+0x108, but their concrete mode dispatch is different.

## Native dispatch precedence (confirmed original ISA)

Both dispatchers load context[+0x9C] (ASM EDI at 0x216204; MODEL ESI at 0x220014), and require actor[+0x18] & context flags.

| Priority | ASM `0x2161F0` | MODEL `0x220000` |
|--|--|--|
| 1 | bit12 => `0x218640` | bit12 => `0x221A40` |
| 2 | mask 0x11 => `0x217370` | bits30/31 => `0x2211C0` |
| 3 | bits30/31 => `0x217970` | mask 0x11 => `0x220D60` |
| 4 | bit18 => skip branch | bit18 => `0x221C10` |
| 5 | bits21/22 => `0x217DC0` | bits21/22 => `0x221450` |
| DEFAULT | `0x2162D0` type5 native packer + alternate packet `RBP+0x20` | `0x2200D0` type5/type6 native packer + default packet `RBP-0x30` |

These are bounded *native dispatcher routes*; no global PTDE operator claim. Other branches have their own native draw code and must not inherit the default LightBank source behavior without census.

## V438 diagnostic scope error

Original V438 `v438_wrapper_payload.py` checked **only** `entity.vtable == image_base+0x12ECA10` before authorizing a typed wrapper invocation. That gate excludes a legitimate `FrpgAsmModelDrawEntity` with vtable `image_base+0x12EBFE8`, even when its type5 native producer `0x2167BA` is entered. Hence V438 could not have given a complete census of armor model sources even if its unsafe wrapper detours had not crashed. This is a **CONFIRMED diagnostic identity coverage gap**, not a confirmed source of visible flicker. V439 disabled those detours; R43 Source2 and original PointLight remain unchanged.

## Reproducibility and limits

`renderer-core/tools/verify_v440_entity_rtti_dispatch.py` verifies actual COL, TypeDescriptor, class base RTTI arrays and dispatch vtables. `verify_v440_dispatch_modes.py` verifies 14 branch opcode preimages and 11 exact CALL targets; seven mutation negatives do not rely on SHA shortcut. Original game EXE is never distributed or modified. **Static RE confirmed**, live actor subclass, frame-mode flags, producer→issued RX34, PTDE semantics and pixel response all OPEN.

The old V413/V414 row30/31 switching is **not** new evidence. New V440 information is the explicit *operator source route, two sibling actor types, exact default modes and per-class packet identity* required to investigate it safely.
