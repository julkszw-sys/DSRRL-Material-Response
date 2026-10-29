# V417 — generational FgModel registry and heterogeneous upstream source

2026-10-29. Renderer Edition, read-only RE and exact diagnostic. Source retail DSR exe SHA256 a45aaa36dd2f6cc151670a639ea5547043cf38ea79ff4178b963c6ed71f98d7b.

## Confirmed native constructor callsites

FgModel ctor RVA 0x76E260 stores caller RDX in parent+0x30 at RVA 0x76E28F. Four instruction-aligned calls:
- 0x27C54F: RDX=RDI, RDI from global 0x141C8A530 +0x10, R8B=1.
- 0x28267B: RDX=RSI, RSI from same global +0x10, R8B=1.
- 0x6A632B: RDX=R14, indexed runtime source at [RDI+RSI*8+0x248], R8D=0.
- 0x72F2C2: RDX=RBP, helper RVA 0x730CE0 result, R8B=1.

A raw E8 byte match at 0x355DF5 is not accepted without a valid native instruction boundary. Caller upstream classes are heterogeneous. The constructor dereferences upstream+0xCA at 0x76E3F0; other consumers access upstream+0x300 and +0x4A0. The upstream pointer is not universally authenticated as a gameplay actor.

## CONFIRMED: native FgModel registration handle has slot plus generation

FgModel ctor calls 0x15A170 at 0x76E40A, saves EAX to FgModel+0x38 at 0x76E40F. This registration path reaches native pool allocator 0x11BA160. Allocation:
- RVA 0x11BA1B0 masks index to low 24 bits (and ebx,0xFFFFFF).
- RVA 0x11BA1B6 retains generation in high byte (and edx,0xFF000000).
- RVA 0x11BA1BE sets bit31 (bts ebx,31), producing allocated handle.

Free/recycle function RVA 0x11BA0E0:
- 0x11BA0FB recovers low 24-bit index.
- 0x11BA10C increments high generation by 0x01000000.
- 0x11BA115 masks high bits with 0x7F000000 (7-bit generation, wraps modulo128).
- 0x11BA11A stores low free-state bit.

Decode allocated handle:
slot = handle & 0x00FFFFFF
generation = (handle >> 24) & 0x7F
allocated_bit = (handle & 0x80000000) != 0

This authenticates a native registry slot+generation, not a permanent globally unique ID, gameplay actor, or pixel consumer. Without registry namespace/lifetime census, it remains a bounded certificate. FgModel+0x40 is a separate refcount initialized at RVA 0x76E29A and changed at RVA 0x76E550/0x76E560. It is NOT a generation.

## V417 diagnostic implementation

The verified FgFlverModel↔FgModel mutual pointers and RTTI from V416 are prerequisites; all unsupported model classes and callers stay unknown. V417 reads parent+0x38 handle and parent+0x40 refcount, and reads first 8 bytes of parent+0x30 upstream if readable; no assumption those bytes are a vtable.

Read-only markers:
[DSRRL V417 FG REGISTRY PROVENANCE]
[DSRRL V417 FG REGISTRY SOURCE JOIN]

The latter logs model, FgModel parent, context, raw handle, refcount, upstream, native Source1 producer sequence, selector rows A/B. An offline analyzer decodes slot/generation and joins exact (context,native_producer_seq) to RX34 row30/31. Negative/missing sample is UNKNOWN. R43 latest_hook_source(), native lighting, CB, shader, material, resource, draw and original EXE on disk unchanged.

STATIC RE CONFIRMED. CONSTRUCTION CI verified separately. IN-GAME V417 SOURCE1/FGMODEL/RX34 join OPEN. GAMEPLAY ACTOR identity OPEN. Render pass/RTV/DSV, CB and PTDE-visible pixel OPEN.

Runtime falsifier: use V417 alone offline, rotate camera with actor standing in direct sunlight, collect ReShade.log; distinguish same or different native model-parent and allocated slot/generation on row30 vs row31. If they match, investigate per-pass CB/RTV routing; if different, investigate contextual owner alias. Neither observation alone proves pixel causality.
