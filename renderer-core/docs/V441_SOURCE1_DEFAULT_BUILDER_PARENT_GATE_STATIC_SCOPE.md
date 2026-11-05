# V441 Source1 default-builder-only parent gate — native RE versus current addon

**Verified exact sources:** GitHub `renderer-core/src/runtime/pmetal_env_source_runtime.cpp` SHA (Git blob) `6a4c4ceed089391eb08da23ac77f8d1a110b18ce`, original retail DSR image SHA256 `a45aaa36dd2f6cc151670a639ea5547043cf38ea79ff4178b963c6ed71f98d7b`. This is a **static coverage limitation**, **NOT a confirmed flicker cause** or request to change the bridge.

## Source1 admission in existing addon

`pmetal_env_source_selector_event(owner,return_address,r14,r15,selector_stack,material)` has:
- exact model selector return RVA whitelist `k_ret_sel_1=0x20E019`, `k_ret_sel_2=0x20EB7F`, `k_ret_sel_3=0x20FB9E`;
- one hard-coded parent identity `k_parent_return=0x220CF0`, checked by reading `selector_stack + parent_offset` (offset0x170 for return1; 0xF0 for returns2/3), and rejecting every other value;
- requires `descriptor_owner == owner` and `model_owner_instance == owner`, where `owner` is native FrpgDrawContext, not a proven gameplay actor/generation;
- emits Source1 cache only *after* above conditions. Failure is fail-open to existing stock/fallback logic, not a direct reason for arbitrary replacement.

Original typed default `FrpgModelDrawEntity` builder `0x2200D0` calls virtual model draw under `0x220CED`, return RVA **0x220CF0**. Original typed ASM `FrpgAsmModelDrawEntity` builder `0x2162D0` calls virtual model draw under `0x2172AA`, return RVA **0x2172AD**.

Therefore the current exact parent gate is **default FrpgModelDrawEntity path only**. It does **NOT** automatically authorize the ASM source/consumer, although original ASM builder emits type5 native LightBank. An ASM-specific Source1 island would need exact typed builder/packet/vtable, model-generation and material/receiver census and PTDE operator evidence before any coverage extension.

Related proven source identity limits:
- Both typed classes have separate native lifetime; pointer reuse across generations is possible but not observed.
- Original `FrpgModelDrawEntity` is also embedded as a subobject of `FrpgFxClusterAppearance_Model` (+0x50) and `FrpgFxParticleAppearance_Model` (+0x30); valid internal vtable alone is not actor-armor identity.
- V439 model candidate FLVER digest `ed0a79b086f1cff26005e0d2c184e269b532046d7cfb83d1eda3bcc3911ac6cc` has no direct parser-length match in 1009 supplied map/character/armor FLVER assets; incomplete archive means gameplay actor identification still OPEN.

**Operator status:** coverage gate CONFIRMED STATIC; ASM runtime opportunity/absence of issued Source1, wrong-assignment, camera flicker and PTDE visible target **OPEN**. Do not globally force Source1 and do not change PROTECTED R43 Source2 fallback. Existing V438 type5/6 wrapper hooks were disabled in V439 after a crash; no new hook in this RE phase.

Repro verifier `renderer-core/tools/verify_v441_source1_exact_parent_gate.py` checks original retail EXE and actual addon source. Local GitHub source check independently found the exact hard-coded constants, owner gate and parent gate once each (all seven exact source predicates PASS). For the game EXE the two virtual call opcodes are verified at 0x220CED and0x2172AA (both `FF 50 18`).
