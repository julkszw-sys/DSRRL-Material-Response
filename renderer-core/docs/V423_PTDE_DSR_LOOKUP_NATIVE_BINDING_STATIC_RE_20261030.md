# V423 — static PTDE 32-bit candidate lookup vs DSR 64-bit LightBank native binding

2026-10-30. **RE-only**. Retail SHA DSR `a45aaa36dd2f6cc151670a639ea5547043cf38ea79ff4178b963c6ed71f98d7b`; PTDE `88d1eec18eba2542c9c7e5e6903e89f9c075b20be5cd045ce6e568e7b22bea9d`. Original executables untouched. No new addon, no PARAM/material gain, frozen V359-RC1 and R43 unchanged.

## PTDE structural lookup candidate — HIGH CONFIDENCE, active routing OPEN

PTDE `VA 0xD31590` / `0xD315B0` (`RVA 0x931590/0x9315B0`): validates area < 12 and type <= 12; area stride `0xD8`, type stride 8; table pointer at manager + area*0xD8 + 0x14; returns first DWORD or second DWORD at +4.

DSR verified `RVA 0x569B40`: validates area <=11 and type <=12; area stride `0x1B0`, type stride 16; manager table pointer at +0x20; returns second QWORD of entry at +8. Ratios `0x1B0/0xD8=2`, `16/8=2`; both 12 areas, 13 types. PTDE `VA 0xD315E0` has compatible 0xD8 indexing and writes area/type metadata.

**Caution:** no direct `CALL rel32` or raw literal VA pointer references to PTDE `D31590/D315B0` found in original file. Indirect dispatch is not excluded. This is **structural homology candidate**, not confirmed PTDE active LightBank consumer, fallback or visible pixel. Do not promote to a PTDE shader operator island.

## DSR exact native cache consumer — STATIC ABI CONFIRMED

At `0x1EB479`, `0x1EB486`, `0x1EB497`, `0x1EB4A3`, original native `0x1EB320` checks four fields: `FrpgDrawContext +0x2348` (A WORD), `+0x234A` (B WORD), `+0x234C` (arg3 DWORD), `+0x2350` (arg4 DWORD). Any mismatch branches to `0x1EB539`, updating all four.

Cache **hit** does not re-lookup bank but still invokes `0x1EAC30` with indices 11,12, and `0x0BADC0` with remembered handles in `+0x2354/+0x2358`. Therefore `cache hit == no state binding` is false.

Cache **miss** calls `0x569B40` bank lookup (exact rel32 at `0x1EB582/59D/5C6/5E1`), extracts record identifiers by `0x563F90/0x564000` (native quotient and remainder using 10000, `0x2710`), resolves resource handles `0x1EAD10`, then `0x1EAC30`/`0x0BADC0` for indices 11–14. Handles are cached at `FrpgDrawContext+0x2354/+0x2358`.

`0x1EAC30` accesses backend through context +0x80/+0x90, tests its cached resource pointer and invokes virtual backend slot `+0x50`. `0x0BADC0` stores per-native-index state in context +0xD4/+0x118, and dispatches virtual backend `+0x40` through `0x0BAD60`. **Native index 11–14 IS NOT YET ATTESTED as D3D11 PS t11..t14 or CB b11..b14.** First resolve the concrete backend vtables and actual D3D11 calls.

`0x1EB806` feeds `0x1EB814 -> 0x1C0E40`, where one exact 16-byte vector from R8 is copied to `[FrpgDrawContext+0x1720]`. This is a concrete CPU-side float4 transport; CB upload and shader consumer OPEN.

The alternative native operator `0x1EADE0` shares the final binding helpers but not the same bank-selection equations/cache path. Separate receiver scope.

## Negative evidence, open actions

Exact direct/literal scan DSR `0x56F4D0` temporal writer also found zero references; cannot certify it updates the instance read at `0x2200D0 +0x110/+0x112/+0x114`. Historical V413 row30↔row31 does not attest native runtime branch E0/F0, GPU slot, frame/RTV, or armor pixel. Other PTDE-vs-DSR shader/gamma/probe-alpha/saturation operators are independent from the LightBank key.

Next STATIC tasks before runtime: trace backend virtual slots +0x40/+0x50 to D3D11 binding and CB upload (including context +0x1720); locate actual PTDE active caller and D3D9 consumer or disprove candidate; certify writer-to-full-builder object identity, all model/receiver families. After those reach a static limit, only precise runtime witness should fill per-instance/pixel gaps.

Reproduction artifact: `DSRRL_V423_STATIC_LOOKUP_BINDING_RE_FULL_HANDOFF_2026-10-30.zip` includes detailed report, stdlib Python verifier with **23 original DSR opcode preimages, 4 PTDE preimages, 30 original DSR call-target assertions**, SHA checks and JSON. No proprietary game binaries.

STATUS: PTDE structural candidate HIGH CONFIDENCE / active routing OPEN; DSR native cache and CPU binding transport CONFIRMED static; direct D3D binding/CB/RTV OPEN; PTDE visible pixel OPEN.