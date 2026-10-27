# V403 — native LightBank staging direct-reference census

Pinned EXE SHA256 a45aaa36dd2f6cc151670a639ea5547043cf38ea79ff4178b963c6ed71f98d7b. Static RE only; runtime/pixel OPEN.

Across both PE .text sections, RVAs 0x1240..0x12B0 at stride 0x10 occur in 164 raw displacement byte matches. Narrow llvm-objdump windows recover 131 memory-reference instructions: 57 non-stack direct writes, 1 non-stack LEA, 0 non-stack direct data reads; 59 stack/frame writes, 7 reads, 7 LEAs. This is a direct-displacement candidate census, NOT complete GPU consumer proof: indirect aliases/copies remain possible.

Independent writer RVA 0x4FFF10 takes destination from RDX into RSI. At 0x4FFFAC it loads a selected source from [RDI+0x60], tests it and on NULL executes XORPS XMM1,XMM1 then writes zero to [RSI+0x1240] at 0x4FFFC2, +0x1250 at 0x4FFFC9 and +0x1260 at 0x4FFFD0. Another path writes the same three offsets (0x50009A,0x5000A1,0x5000AF). This code is not yet proven to run for RX34 or even the specific LightBank actor type. No direct E8 rel32 caller to 0x4FFF10/0x5000E0 was found; indirect dispatch OPEN.

The lone non-stack LEA at 0x120BB24 passes [RDX+0x1260] to an apparent generic thunk at 0x54EB00; actor identity and CB consumer OPEN. Reverified direct native callsites: wrapper 0x1C0BE0 at 0x2167BA/0x220639; 0x1C0C10 at 0x220632; native single 0x563B80 four callsites, blend 0x563C30 two callsites.

RE result: cannot equate last packer output, mutable actor staging and actual GPU CB. Next isolate indirect writer dispatch and trace exact actor-state serialization/CB upload. No source gate, row veto, heuristic, or EXE modification; protected latest_hook_source remains unchanged.

## V403 RTTI correction / attribution

The `.rdata` entry containing RVA 0x4FFF10 at vftable slot 21 and 0x5000E0 at slot 22 belongs to MSVC RTTI type `.?AVShaderContext_FrpgFxNormalEntity@NS_FRPG@@`, i.e. `NS_FRPG::ShaderContext_FrpgFxNormalEntity`, **not** to the neighboring `FX4HG::FXHGShaderContextEntity` vftable. Exact Complete Object Locator RVA 0x165FF70, virtual table start RVA 0x136C520; RTTI locator signature=1, self pointer matches. Source class of RCX is thus shader context; RDX destination type, current receiver and uploaded CB consumer remain OPEN. The earlier hypothetical identification as an actor updater must not be asserted.
