# V441 native actor source identity: VFX parent class and nested ModelDrawEntity

**Original retail DSR PE SHA-256:** `a45aaa36dd2f6cc151670a639ea5547043cf38ea79ff4178b963c6ed71f98d7b`. Only read-only original instruction/RTTI analysis. Runtime/pixel remains OPEN.

The direct rel32 caller census for `FrpgModelDrawEntity` constructor RVA0x21F900 is exactly three call sites in the two original .text sections: 0x223B0F allocates standalone 0x1C0-byte entity; 0x4FD3DE and0x4FE7FE invoke it on a **nested subobject** of independently typed FX parents. Direct call sites for `FrpgAsmModelDrawEntity` ctor0x215960: only factory0x21AF8F, separate allocation0x250 bytes. Indirect calls not excluded.

Original MSVC RTTI:
- Outer vtable 0x136C0D0 → COL0x165FBE0 → TypeDescriptor0x1B107A8 = `FrpgFxClusterAppearance_Model`, base `FFX::FXClusterAppearance_Model` and `FFX::FXClusterAppearance`. Constructor 0x4FD390 passes `RCX=[outer+0x50]` at 0x4FD3CC → call 0x4FD3DE `FrpgModelDrawEntity::ctor0x21F900`. Destructor 0x4FC050 passes `outer+0x50` at0x4FC09A to inner destructor0x21FBD0 at0x4FC09E. Embedded inner DrawParam is `outer+0x158`, inner model pointer is `outer+0x90`.
- Outer vtable0x136C208 → COL0x165FD10 → TD0x1B10850 = `FrpgFxParticleAppearance_Model`, bases `FFX::FXParticleAppearance_Model` and `FFX::FXParticleAppearance`. Constructor0x4FE7B0 at0x4FE7EC/0x4FE7FE passes `outer+0x30` to inner ModelDrawEntity constructor. Destructor0x4FDC30 at0x4FDC7A/0x4FDC7E passes same offset to destructor. DrawParam is `outer+0x138`; model pointer `outer+0x70`.

Reproducibility: `verify_v441_nested_vfx_model_actor.py` validates RTTI COL/TD/base arrays, original vtables, inner offset, exact ctor/dtor calls; `test_v441_nested_vfx_model_mutations.py` checks 10 in-memory semantic mutations, all PASS. No EXE mutation on disk.

**Implication:** a valid inner `FrpgModelDrawEntity` vtable or matching address is **not sufficient** to certify that the Source1 packet belongs to a gameplay actor or the player's armor. A correctly fail-open receiver-first bridge should track the **outer source class and generation** as well as inner model/packet identity where this path applies. This is a proven model identity scope gap, not proven flicker root cause. V439 model pointer0xE979970 asset identity, actor type and pixel remain OPEN.
