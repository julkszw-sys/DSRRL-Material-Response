# V406 FrpgDrawContext scalar fallback and ShaderContext cache coherence RE

Pinned unmodified EXE SHA256 `a45aaa36dd2f6cc151670a639ea5547043cf38ea79ff4178b963c6ed71f98d7b`, PE64 base `0x140000000`. Static native evidence only; Source1/Source2 flicker causality, RX34 draw/CB and pixels OPEN. Full pinned 53-check verifier and disassembly report in V406 audit ZIP.

## Native initialization

Factory `0x1EC95F` requests `0x2500` bytes, alignment `0x80`, then calls `FrpgDrawContext` constructor `0x1EA460` at `0x1EC9CD`. This is per-factory-call allocation, NOT proof of gameplay actor or receiver identity. Constructor stores native wrapper `+0x2318` and manager `+0x2320`. **Correction to V405:** `+0x2338..+0x2344` are four DWORD fallback values initially zeroed, then conditionally populated by area11 native bank sources. Native pointer getter `0x569B40`, followed by `DWORD [source+0x10]`, seeds: type5→`+0x2338` (call `0x1EA5C5`), type2→`+0x233C` (`0x1EA5E8`), type7→`+0x2340` (`0x1EA60B`), type9→`+0x2344` (`0x1EA62E`). The `0x569B70` ID getter instead returns DWORD `[table+type*16]`; the `0x569B40` getter returns pointer `[table+type*16+8]`. No equality assumed between fallback `[source+0x10]` and live entry DWORD.

## Per-type value fallback

Helper type5 `0x1EAAF0`→`+0x2338`, type2 `0x1EAB30`→`+0x233C`, type7 `0x1EAB70`→`+0x2340`, type9 `0x1EABB0`→`+0x2344`; **type6 `0x1EABF0`→`+0x2338` (type5 bootstrap)** when live numeric type6 getter returns zero. This is a *numeric ID* rule and must not be replaced by a generic last-packaged LightBank pointer.

## Partial update and stale-destination hypothesis

`ShaderContext_FrpgFxNormalEntity` method21 checks +0x68 source-ID at RVA `0x4FFF8B`, updates ID at `0x4FFF90` and source pointer +0x60 at `0x4FFFA8`. Its independent record index +0x6C at `0x4FFFDC` can skip `0x563460` decoding/writes (branch `0x4FFFDF`). Method22 similarly updates ID/pointer `0x50016B..0x500188` then compares independent index +0x70 (`0x5001C4`) and may skip writes (`0x5001C7`). Neither ID-refresh block resets its index. Thus if source ID changes while record index remains equal, a decoder skip is possible. Whether an upstream caller resets destination/cache or RX34 uses this path is not established. `FrpgDrawContext` identity is not automatically gameplay actor identity.

No modification to Source2 R43 protected `latest_hook_source`, materials, lighting equations, shader code, EXE or addon. Next test: join context pointer+native wrapper+producer sequence+receiver RX34+issued draw+CB and observe source-ID changes alongside index/writes. Until then STATIC RE PASS, compatibility/runtime/pixels OPEN.
