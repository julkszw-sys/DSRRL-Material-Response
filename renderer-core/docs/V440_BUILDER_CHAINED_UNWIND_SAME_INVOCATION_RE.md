# V440-R2: default builder same-invocation frame via chained .pdata

SHA-locked retail DSR EXE: `a45aaa36dd2f6cc151670a639ea5547043cf38ea79ff4178b963c6ed71f98d7b`.

Status: **CONFIRMED original static ABI; runtime stack reconstruction and issued RX34/pixels OPEN.**

## Same builder ancestor verified on both sides

Default FrpgModelDrawEntity builder RVA `0x2200D0` prologue `mov r11,rsp`, `lea rbp,[r11-0x238]`; original actor in RDI, stack draw packet `RBP-0x30` (builder entry RSP-0x268), model pointer actor+0x40.

**Source/producer:** type5/type6 wrapper return RIP `0x220637/0x22063E` has exact Win64 chained unwind records:

`(0x2204C3,0x22087A,0x174F560)` ->
`(0x22012A,0x220450,0x174F534)` ->
`(0x220107,0x22012A,0x174F51C)` ->
`(0x2200FE,0x220107,0x174F508)` ->
**builder root** `(0x2200D0,0x2200FE,0x174F4E8)`.

**Draw consumer:** return RIP `0x220CF0` from virtual model draw call:

`(0x220C51,0x220D50,0x174F5C8)` ->
**SAME builder root** `(0x2200D0,0x2200FE,0x174F4E8)`.

**Model shader selector:** `0x20E014 call 0x22BA20` return `0x20E019`:

`(0x20DE68,0x20E652,0x174B548)` ->
`(0x20DDC5,0x20DE68,0x174B51C)` ->
model root `(0x20DDA0,0x20DDC5,0x174B508)`.

All original `UNW_FLAG_CHAININFO` records and direct roots (flags=3) verified with exact bytes and 9 independent semantic mutation negatives. Tool: `renderer-core/tools/verify_v440_builder_unwind.py` (original exe supplied separately by local user, not redistributed).

## First safe native-observer cut

Only existing native packer hooks `0x563B80/0x563C30` and existing typed shader-selector hook `0x22BA20` are needed *in principle*. A Win64 unwinder must prove actual parent producer emitter R14 (type5)/R15 (type6) = original embedded `FrpgDrawParam`, recover the **same builder activation**, its `RBP-0x30` packet and RDI actor, and compare with model RDI/R15 packet in `0x20DDA0`, then exact material/issued RX34. No fresh V438 wrapper detours. Fail open if any unwind level or actor generation differs; never infer joining on same model pointer or last producer alone.

Note this is a **structural candidate**, not a proven live or pixel join: post-producer optional prepass `0x220839 call 0x20FEE0`, transitive packet aliases, alternate type5 path `0x2167BA`, opaque gameplay writer, and deferred GPU issuing remain unproven. The V413 row30/31 changes are previously known, not new discoveries.

Additional original full .text raw relative-call census: `0x1D7820` setter 15 candidate sites, `0x1D77E0` tick 7, `0x1D7530` three-channel dispatcher 2 (`0x219D8F`,`0x222D5F`), `0x1D7580` triple setter 3 (`0xC0240`,`0x219B2B`,`0x222BBB`). Object identity must be validated for each call: others target entity+0x1F0 / +0x164 or non-actor objects. **No evidence that camera rotation invokes setter on player armor.**

No EXE modification, no patch to Source2 R43, and no V440 addon compiled; only reproducible static RE.
