# V421 — Native FlverModel packet + material-element loop (static RE, 2026-10-29)

Basis: original retail DSR EXE SHA256 a45aaa36dd2f6cc151670a639ea5547043cf38ea79ff4178b963c6ed71f98d7b. The preceding V420 verified 224 Source1 producer sequences with two RX34 material-slot draws each and alternating raw selectors 0x051E/0x051F. This report proves additional original native control flow; no new addon or pixel bridge.

* Both FgFlverModel vtable RVA 12EB148 and FlverModel vtable RVA 12EB778 contain VA 14020DDA0: shared virtual draw method at slot +0x18. This is shared code, **not** shared actor or per-packet semantics.
* RVA 20DDE4 moves argument RDX to R15 (draw packet). RVA 20DDFF reads FrpgDrawContext at packet+0.
* RVA 20DE80 reads packet+0x98 inside the model material loop. At 20DF70/20DF86 native code reads/compares packed packet selectors B(+0x4E) and A(+0x4C).
* RVA 20DF92 SETNE R12B produces the A!=B flag, and RVA 20DFFB passes it to material-dispatch stack arguments. In the 448 observed target RX34 draw samples A==B and beta=0, so this mismatch-bit path is **always zero**. It cannot explain their row30/row31 difference. Other packet flags or other consumers are not ruled out.
* RVA 20E014 calls material selector 22BA20; RVA 22BA74 calls separate material-param lookup 295F50. This selector does not synthesize the raw packet A/B field values.
* RVA 20E61C advances the element pointer by 0x1E0 bytes; RVA 20E623 decrements iteration count; 20E631 JNE loops back to 20DE80. R15 packet pointer is retained across that per-element loop; it is restored from the stack at 20E659 **after loop**.

Consequences: two issued RX34 slots per Source1 sequence can naturally arise from one native per-invocation draw packet processed across material elements. The V413 runtime does not expose packet addresses, and therefore does not certify this relationship for its specific two slots. Even if packet pointers coincide, per-draw invocation/lifetime and visibility are separate concerns.

Verification: 17 byte-exact instruction preimages; three direct call targets; one loop-back target; two vtable entries. V420 runtime verifier still confirms 224 producer sequences and 448 issued RX34 events. No shader/CB/RTV or visible pixel inference.

Next read-only witness: for exact 20E019 caller, validate native packet[0]==owner and snapshot packet address, A/B/beta and relevant flags; copy to Source1 journal keyed by exact producer sequence/epoch, then compare against post-mutation/pre-DrawIndexed V419 PS/CB12 window/RTV/DSV. Never authorize a lighting fix based solely on packet equality.

Status: STATIC NATIVE ABI CONFIRMED; historical V413 observational pattern CONFIRMED; V421 hook/runtime/actor/pixel OPEN. V359-RC1, R43 and EXE unchanged.

## V413 timing correction — CONFIRMED by re-parsing original log

The 224 producer sequences form 112 adjacent row30->row31 pairs. **107 of the 112 pairs have identical GetTickCount64 tick_ms**, while the remaining five differ by 15/16ms. Within 224 producer sequences, each emits ordered material slots 0->1, and **219 of 224 slot pairs share identical tick_ms**. All 448 exact source/draw associations remain matched. Thus the data do NOT support a simple claim that alternate rows exclusively occur on different display frames. The two states may be emitted in different render passes in one cycle, but no frame/RTV/DSV identity is observed in V413. GetTickCount64 equality is only a coarse CPU timestamp, not GPU frame identity.

**Operational recommendation:** capture per-invocation draw packet and verify exact 20E019 provenance; join to V419 pre-native-DrawIndexed PS, CB12 window, RTV/DSV and indexed geometry before concluding the bright row should be chosen. NO row pinning or LightBank gain patch. No new V421 .addon64 was built or runtime tested.
