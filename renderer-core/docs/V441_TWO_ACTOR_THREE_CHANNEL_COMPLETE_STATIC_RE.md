# V441 two-actor / three-channel source-transport RE (static-only)

Parent: V440 original retail DSR SHA256 `a45aaa36dd2f6cc151670a639ea5547043cf38ea79ff4178b963c6ed71f98d7b`. Original EXE analyzed **read-only**, never distributed or altered. V439 remains last built integrated addon. **No V441 addon, shader patch, PARAM change, GPU/pixel pass.**

## I. Bounded call census, two actor classes

Original MSVC RTTI proves siblings `FrpgAsmModelDrawEntity` vtable+0x50→RVA 0x2161F0, `FrpgModelDrawEntity` →0x220000, both inheriting `FrpgDrawEntity`. Their native dispatcher modes are *different*, do not transfer a LightBank policy from one to the other.

Both original .text sections RVA 0x1000 and 0x2019000 scanned for raw E8 rel32 direct calls; full exact source+prologue/owner witnesses and tests:
- Type5 wrapper 0x1C0BE0: 0x2167BA (ASM default), 0x220639 (MODEL default).
- Type6 wrapper 0x1C0C10: 0x220632 (MODEL default).
- Original single packer 0x563B80: 0x1C69FE,0x1C6A97,0x1C6D55,0x1C6DEE.
- Original blend packer 0x563C30: 0x1C6BC5,0x1C6F4A.
- Three-channel ticking 0x1D7530: 0x219D8F,0x222D5F.
- Triple channel setter 0x1D7580: 0xC0240,0x219B2B,0x222BBB.
- Model prepass 0x20FEE0: 0x210533,0x216C13,0x220839.
- Shader selector 0x22BA20: 0x20E014,0x20EB7A,0x20FB99. **Shader selection is not a physical LightBank row**.
- Specialized native render target 0x20E9D0: five direct callers including ASM 0x217913/0x217D5D and MODEL 0x22119B/0x221412; 0x20F9F0 from 0x222033. Virtual model.vtable+0x18 calls at 0x2172AA and 0x220CED need exact vtable validation for dynamic models.

Direct rel32 **does not** eliminate indirect calls or obfuscated transfers.

## II. Native setter identity — avoid false LightBank provenance

Same helper `0x1D7820` is called by different object families. Semantically checked emitter-owner near each call (not just identical target):
- actor DrawParam three channels `entity+0x108` via 0x219D8F and 0x222D5F.
- other actor fields 0x219CF8 operates +0x1F0; 0x223068 operates +0x164.
- another context: 0x2D39C0/0x2D39E6/0x2D3A0C/0x2D3A32/0x2D3A58 target respective +0x1B0,+0x1C0,+0x1D0,+0x1E0,+0x1F0.
- 0x55D05C/0x55D084 target separate +0x58 channel.
**Camera movement → exact player's setter invocation remains OPEN.** Never infer causal ownership from a common function address.

## III. Original typed actor lifecycle

`FrpgAsmModelDrawEntity`: ctor RVA0x215960, embedded DrawParam ctor on actor+0x108 →0x1D7490 at 0x215A29; destructor 0x215D20 destroys that DrawParam via 0x1D74F0 at 0x215DED; scalar deleting destructor 0x21AA30, allocation/deallocation object size 0x250. Its concrete vtable 0x12EBFE8 becomes base 0x12AA868 during teardown.

`FrpgModelDrawEntity`: ctor RVA0x21F900, embedded DrawParam ctor at 0x21F9C5, destructor 0x21FBD0, embedded DrawParam destructor at 0x21FC00; scalar deleting destructor 0x2237E0, object size 0x1C0; vtable 0x12ECA10 becomes base 0x12AA868. Both model ptr actor+0x40.

**Consequence:** an actor address is not a lifetime-stable generation ID. This proves possibility of pointer reuse, **not runtime reuse actually observed or the flicker cause**.

## IV. THREE independent native DrawParam channels

Default native builders of both actor classes invoke three distinct source transports. `FrpgDrawParam=actor+0x108`.
1. First channel dp+0x08: native type5/type6 LightBank; base source/caller and row 30/31 known from V413/V414. **Do not count as newly discovered**.
2. Second channel dp+0x18: wrapper 0x1C0C40 (caller 0x2167CC / 0x220647), tailcall 0x1C8650; category2 resource, beta dp+0x1C, cache `FrpgDrawContext+0x228C`, output `+0x290` and `+0x12F0`.
3. Third channel dp+0x28: wrapper 0x1C0C60 (caller 0x2167DE / 0x220655), tailcall 0x1CAC50; category7 resource, beta dp+0x2C, cache context+0x2290. Exactly **32 SIMD stores**, 16 in each single/blend branch, to context+0x2A0..0x310 and +0x1300..0x1370. Identical physical offsets in unrelated objects are not enough to infer consumer.

The **CPU-context → GPU CB/SRV upload → exact receiver shader/material → visible PTDE operator** for channels2/3 remains OPEN. Do not rename them "GI", "ambient", "specular", or apply brightness/probe compensation until consumer census.

## V. Verification and remaining frontier

Local original-exe static verifiers passed; negative semantic mutations: lifecycle10, channel2/3 18, source/owner census14 = **42**; all based on instruction/RTTI/owner bytes, no SHA-only shortcuts. GitHub static verifiers `verify_v441_two_actor_source_census_r2.py`, `verify_v441_entity_lifecycle.py`, `verify_v441_three_channel_transport.py` and `test_v441_all_semantic_mutations.py`. R2 supersedes initial `verify_v441_two_actor_source_census.py` (its inherited PE interface was incompatible; not authoritative).

The nearest unresolved *runtime* cut: exact same-invocation native producer/actor/packet via validated RtlVirtualUnwind within existing 0x563B80/0x563C30 packer detours, typed model vtable +0x18, packet post-prepass and issued RX34. Do not re-enable V438 crash-suspect wrapper detours; retain R43 Source2 and stock independent operators. No permanent bridge before **actor generation + exact issued draw + pixel** verification.

Status: STATIC RE CONFIRMED for scoped routes, source ownership and independent CPU operators; BUILD V441 N/A; RUNTIME LIVENESS/BRIDGE ACTIVATION/PIXEL OPEN.
