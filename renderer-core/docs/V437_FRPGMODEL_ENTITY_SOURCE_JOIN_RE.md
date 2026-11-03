# V437 native LightBank producer provenance (static DSR RE)

Base: V436 head 70eaba809a688ddcf51f537ea0751fefebb8c90e. Original owner DSR PE x64 SHA256 a45aaa36dd2f6cc151670a639ea5547043cf38ea79ff4178b963c6ed71f98d7b. No game executable edits.

Confirmed original x64 call graph:

- Main builder 0x2200D0 keeps FrpgModelDrawEntity in RDI; packet at RBP-0x30.
- A/B copied from entity+0x110/+0x112 by pure getters 0xC04A0/0xC0550 to packet+0x4C/+0x4E. Beta entity+0x114 to packet+0x50.
- 0x220614 calculates RBX=entity+0x108; 0x22062B passes RBX in R8 to native producer wrapper.
- When entity[0x39]&0xF equals 4: 0x220632 calls 0x1C0C10 -> 0x1C6C40 (type6). Otherwise 0x220639 calls 0x1C0BE0 -> 0x1C6900 (type5).
- Both wrappers pass R8 to R9. Both emitters use pure getters at R9+8, the same embedded entity+0x110 fields. Flag also stored at packet+0x150 at 0x2203E0.
- Native single 0x563B80 is called at 0x1C69FE, 0x1C6A97, 0x1C6D55, 0x1C6DEE; native blend 0x563C30 at 0x1C6BC5 and 0x1C6F4A.
- Same builder subsequently passes model=[RDI+0x40] and packet=[RBP-0x30] to model.vtable+0x18 at 0x220CED (typed 0x20DDA0).
- Separate native type5 wrapper call 0x2167BA also supplies its entity+0x108; do not infer identity from global last event.
- Optional prepass 0x220839 -> 0x20FEE0 means after-prepass packet mutation is not ruled out.

Original static byte and direct-target census passed: 43 instruction preimages, 20 calls, 6 exact direct target sets. Native producer sequence to specific model invocation, post-prepass data, same-issued RX34 and PTDE pixel all OPEN. Next valid bridge is a read-only original-wrapper invocation certificate connected to existing V411-V419 journaling. Protected R43 Source2 fallback remains unchanged. No addon build in V437.
