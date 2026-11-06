# V442 — typed FrpgDrawContext post-producer mutation, exact original DSR RE

**Target:** original, unmodified retail `DarkSoulsRemastered.exe` SHA256 `a45aaa36dd2f6cc151670a639ea5547043cf38ea79ff4178b963c6ed71f98d7b`. **Parent:** V441; inherits V439 one-addon but this is static RE only, no new injected runtime. PTDE pixel and observed armor identity remain OPEN.

## Same context before and after native LightBank

In normal `FrpgModelDrawEntity` builder 0x2200D0, the incoming RDX context is retained in RSI by 0x220102 and packet[0] is stored at 0x2201A6. All three emitter calls pass RDX=RSI: type5/type6 0x220639/0x220632, channel2 0x220647 and channel3 0x220655; they use R8=entity+0x108. Later `call [model.vtable+0x18]` at 0x220CED gets that packet.

In sibling `FrpgAsmModelDrawEntity` builder 0x2162D0, original RDX context is retained in R14 by 0x21630E, packet[0] at RBP+0x20 is assigned R14 by 0x21639C, and the three corresponding calls 0x2167BA, 0x2167CC and 0x2167DE all pass RDX=R14 with R8=entity+0x108. Its packet is subsequently passed at 0x2172AA.

In actual typed receiver 0x20DDA0, 0x20DDE4 captures packet pointer in R15; `0x20DDFF mov r14,[r15]` reads **the same original context pointer**, and `0x20E106 call 0x1C08E0` later passes RDX=R14. This is a structural transport certificate, not a game actor generation or final GPU draw certificate.

## Exact direct calls to context-mutating 0x1C08E0

Both original executable .text sections were scanned for direct E8/rel32 callsites; exactly four appear:

| site | enclosing function | RDX provenance | context timing |
|---|---|---|---|
| 0x210195 | 0x20FEE0 | RDI = packet[0] via 0x20FF69 | prepass, conditionally before actual model draw |
| 0x20E106 | 0x20DDA0 | R14 = packet[0] via 0x20DDFF | typed primary model material/subpart loop |
| 0x20EC38 | 0x20E9D0 | RBP = packet[0] via 0x20E9FF | specialized model receiver |
| 0x20FC57 | 0x20F9F0 | RBP = packet[0] via 0x20FA23 | second specialized receiver |

All four targets are exact and share the same `0x1C08E0` body. No global absence of **indirect** callers is claimed. A prepass may skip, material count may be zero, and different model receiver families take different control-flow paths.

## What the downstream operator actually writes

Inside `0x1C08E0`, R12=RDX context, R14=R9 descriptor, R15=R8 input. The byte `[R9+5]` controls the loop count. The destination is computed using `EBX=0x1C`, `RCX=EBX+0x22`, `RCX<<=4`, `RCX+=R12`, followed by three `movups` writes at +0,+0x10,+0x20, with EBX increasing by 3. Thus **context+0x3E0+0x30*i**, 48 bytes per active element. Alternate branches also use 3x16 vector writes. These look like indexed matrix/vector palette transport but their exact GPU consumer/meaning is not yet proved; do not classify as LightBank brightness, diffuse, specularity or gamma.

No global max for `[R9+5]` has been certified. Accordingly, avoid unconditional claims about all possible destination ranges or overlap.

## Distinct second and third channel cache

Channel2 original helper `0x1C8650`: `cmp [ctx+0x228C], A` at 0x1C86A4 and `mov [ctx+0x228C],A` at 0x1C871C; resource category2, CPU output `ctx+0x290` and `ctx+0x12F0`.

Channel3 original helper `0x1CAC50`: `cmp [ctx+0x2290],A` at 0x1CACA7 and `mov [ctx+0x2290],A` at 0x1CAD9F; category7, CPU output vecs `ctx+0x2A0..0x310` and `ctx+0x1300..0x1370`. Cache keys shown here are selector-only fields in the shared context — **not actor generation or proof of collision**. Exact logical resource identity and GPU CB/SRV upload still OPEN.

## Diagnostic and patch decision

V413/V414 already established row30↔31 switching. **New V442 result** is the downstream mutation/caller census: native LightBank producer and packet equality cannot prove which modified context or material vectors a particular issued RX34 consumed. The narrowest live diagnostic must link: actor subclass + generation → builder frame/packet → native producer sequence + channel1/2/3 selector state → conditional prepass → correct typed model receiver/subpart → issued RX34 PS/CB/SRV/RTV. Unmatched cases fail open. No blanket gain, row clamp, exposure, shader disable, R43 Source2 change, or EXE-on-disk mutation.

## Reproducibility

The current-conversation source archive `DSRRL_V442_CONTEXT_POSTPRODUCER_EXACT_RE_2026-11-06.zip` has SHA256 `83e5589f53d7a0b033b348ab243cb3d3ee6fd6295bf126d8a508d8edc5ba7c1e`. Includes full standalone verifier `v442_verify_context_identity.py` (47 exact opcode witnesses, 11 direct targets, full direct caller census, 27 *semantic* mutation controls independent of SHA shortcut), independent `v442_verify_material_context.py` (46 witnesses, 7 calls, 18 negatives), report and output. Original EXE is not distributed.

**Construction: STATIC_RE_CONFIRMED**; **Compatibility, runtime liveness, bridge activation and pixel: OPEN**. No V442 addon build is claimed.
