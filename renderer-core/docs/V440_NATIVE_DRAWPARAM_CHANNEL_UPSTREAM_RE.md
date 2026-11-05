# V440 native LightBank channel state machine and typed producer ABI (static RE)

**Parent:** V439 exact commit `1f6bc31c1b28755ed226d24de51717aba6ce8184`. **Input:** original retail DarkSoulsRemastered.exe, read-only SHA256 `a45aaa36dd2f6cc151670a639ea5547043cf38ea79ff4178b963c6ed71f98d7b`. **Status:** static ISA CONFIRMED, gameplay actor/source-to-RX34/pixels OPEN. No addon behavioral modification; R43 Source2 unchanged.

**NOT NEW:** V413/V414 already logged rapid LightBank row30/31 switching in the same material with shared FrpgDrawContext owner. V440 must not be described as discovering the row toggle.

## Newly reconstructed native channel

The verified FrpgModelDrawEntity embeds FrpgDrawParam at entity+0x108, with three 16-byte channels initialized by `0x1D7490 → 0x1D7750`. First LightBank channel begins entity+0x110. Fields: WORD A +0; WORD B +2; float beta +4; float duration +8; float elapsed +0xC.

`0x1D7820` setter accepts RCX=channel, RDX=&new_WORD, XMM2=duration. With negative duration or invalid selector it collapses A=B, clears beta and elapsed. When new target equals A and duration equals current duration it reverses transition, preserving elapsed continuity. New valid unequal target otherwise updates B and resets beta/elapsed. **Exactly zero duration does not automatically imply immediate reset** (COMISS 0,duration JBE enters the normal branch). `0x1D77E0` advances elapsed/beta and collapses A=B on completion.

`0x1D7580` updates all three channels through three exact calls to `0x1D7820`. Callers `0x219AD0` (RVA 0x219B17/0x219B2B) and `0x222B60` (0x222BA7/0x222BBB) read serialized inputs and target embedded dp+8/18/28. **This does not prove live camera-induced setter provenance.** Calls from 0x219CF8 and 0x223068 target other entity fields (+0x1F0/+0x164), so exclude them from LightBank source graph.

## Exact typed builder to consumer

Default builder `0x2200D0`: same entity RDI; A/B/beta from entity+0x110/+0x112/+0x114 to stack packet+0x4C/+0x4E/+0x50. At 0x220614 the **same** dp=entity+0x108 is passed to type5 wrapper `0x1C0BE0` (call 0x220639) or type6 `0x1C0C10` (call 0x220632). At 0x220CD3/0x220CE6/0x220CED this stack packet and entity[+0x40] model enter model.vtable+0x18. FgFlverModel and FlverModel verified vtable slots 0x12EB148/0x12EB778 both point to `0x20DDA0`. It retains model in RDI and draw packet in R15, reads A/B via packet+0x4C/+0x4E and computes their inequality at 0x20DF86/0x20DF92 before `0x20E014 call 0x22BA20` (shader-set identity, **not physical row**).

Builder `entity+0x14C` is a **separate scalar** from beta; emitter compares it with 1.0 and may multiply output registers; no blanket-gain or material patch authorized. Conditional `0x220839 call 0x20FEE0` prepass occurs before virtual draw. Direct packet A/B/beta writes in the inspected prepass were not found, but transitive mutation via its calls remains OPEN.

## New producer frame witness (NO new wrapper patch required)

Original emitter type5 `0x1C6900`: `0x1C6939 mov r14,r9`, R14 holds original embedded dp while packer calls 0x1C69FE/0x1C6A97 (single `0x563B80`) or 0x1C6BC5 (blend `0x563C30`) occur.

Type6 `0x1C6C40`: `0x1C6C79 mov r15,r9`, R15 holds original dp while packer calls 0x1C6D55/0x1C6DEE (single) or 0x1C6F4A (blend) occur. Existing V383/V400 packer hooks are already installed in integrated addon. This is a potential exact same-native-invocation identity witness **only if** original emitter nonvolatile registers are recovered via validated Windows unwind and vtable/caller/owner-generation verified. Current runtime has NOT certified this unwind. Alternate `0x2167BA` type5 wrapper caller has a different enclosing execution route and must not be conflated with default builder.

## RE limit and next cut

`0x1D7530` first-channel tick dispatcher enters obfuscated control flow in the second .text section; blindly walking the following bytes as a normal function would be false RE. The original gameplay setter, upstream actor generation, packet post-prepass mutation, exact issued RX34 and PTDE pixel operator remain OPEN.

Local reproducibility: independent handoff contains `verify_v440_native_re.py` (69 opcode preimages, 21 rel32 call edges, 2 excluded other-object calls, 3 vtables), `test_v440_mutation_guards.py` (10 adversarial corruptions), results JSON and full analysis. Original EXE not distributed. No V440 runtime addon; build and pixel status not promoted.
