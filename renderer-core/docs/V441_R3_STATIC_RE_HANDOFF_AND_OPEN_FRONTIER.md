# V441 R3 static RE handoff (original DSR, PTDE target remains OPEN)

**Provenance:** original unmodified retail DarkSoulsRemastered.exe SHA256 `a45aaa36dd2f6cc151670a639ea5547043cf38ea79ff4178b963c6ed71f98d7b`; GitHub branch `agent/v441-complete-direct-emitter-setter-actor-dispatch-census-20261105` based on V440 and V439 integrated addon. No EXE redistribution/modification, no new addon build.

**Offline reproducible source archive:** `DSRRL_V441_R3_EXHAUSTIVE_NATIVE_SOURCE_STATIC_RE_HANDOFF_2026-11-05.zip`, ZIP container SHA256 `77d12d12bfe43787a5b0b4c34a3fd49f8ff7d083acc4256fa1b3fb7ac5af2307`, 32252 bytes, 30 files, ZIP CRC PASS; includes V441 R2 existing SHA-protected original-EXE opcode/RTTI/call verifiers, 52 semantic mutation negatives, Source1 static parent gate independent verifier, source gate predicate report, and 1009-FLVER bounded asset audit. Archive available in current conversation; no game EXE inside.

**Net new static findings:**
1. Original native sibling actors `FrpgModelDrawEntity` and `FrpgAsmModelDrawEntity`, different mode dispatchers and builder packet layouts, exact native channel type5/6, independent actor lifetime.
2. Three `FrpgDrawParam` CPU channels: channel1 known LB, channel2 category2 operator0x1C8650, channel3 category7 operator0x1CAC50 with 32 SIMD writes, GPU receiver unknown.
3. Original FX cluster and particle outer parent classes embed a `FrpgModelDrawEntity` at +0x50/+0x30; inner model vtable cannot certify armor/gameplay owner.
4. Exact current Source1 code hard-gates parent return0x220CF0, matching default builder and **excluding** alternate ASM parent return0x2172AD. This is a **policy scope observation**, not evidence of wrong behavior. Owner equivalence verifies only FrpgDrawContext identity, not actor generation.
5. V439 model FLVER SHA had zero matches in 1009 partial uploaded original assets; does not imply candidate not armor.
6. Both original .text direct E8 rel32 source-call census and setter-other-object exclusions, not an indirect call census.

**Static constraints:** exact original source byte witnesses, actor lifetime and class RTTI, 52/52 adversarial semantic mutation controls all PASS. Related Source1 code predicates 7/7 PASS in the connected GitHub source. Correct old V413 row30/31 switching is pre-existing, **not a new V441 finding**.

**Unsolved required proofs:** camera-triggered upstream actor setter; proper same-invocation native producer→actual actor/generation→typed FLVER packet→issued RX34 material; GPU constant-buffer/resource consumer for channels2/3; PTDE visible operator comparison and pixel equivalence. No gameplay/runtime or pixel PASS. A safe future observation island may use existing packer hooks + original verified Win64 RtlVirtualUnwind; never re-enable V438 crash-suspect wrapper detours or globally change protected R43 Source2. No canonical lighting/routing patch authorized.

For per-operator detail see adjacent V441 README and verification files.
