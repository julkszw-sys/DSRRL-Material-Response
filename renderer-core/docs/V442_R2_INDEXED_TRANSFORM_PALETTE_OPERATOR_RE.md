# V442-R2 — indexed 3x4 transform palette, not a LightBank row operator

**Original DSR unchanged:** SHA256 `a45aaa36dd2f6cc151670a639ea5547043cf38ea79ff4178b963c6ed71f98d7b`. Static RE only; no V442 addon/runtime/pixel claims. Original V442 established 0x1C08E0 has four direct callsites: prepass `0x210195`, typed model `0x20E106`, specialized models `0x20EC38`, `0x20FC57`.

R2 independently reconstructs the indexed operator source-to-output data path:
- `0x1C0916 movzx ebx,byte [r9+5]`: descriptor element count; indices start at `r9+6`.
- `0x1C09B0 movzx ecx,word [r14]`; `0x1C09B9/BC`: validate current 16-bit index against `[rdi+8]`.
- `0x1C09C1/C9`: remap through `word [[rdi+0x10]+2*index]`; invalid index takes the -1 sentinel branch.
- `0x1C09DE/E2/E6`: retrieve indexed 48-byte transform block at `[r15+0x10]+0x30*mapped_index`.
- `0x1C09EA call 0x194030 → 0x194E80`, where `shufps/mulps/addps` implement 3x4 matrix composition.
- `0x1C09FB call 0x1C1EF0 → 0x1C1F90`, three vector `addps` operations.
- `0x1C0A06/0D/14`: destination `FrpgDrawContext+0x3E0+0x30*i`; `0x1C0A17/1E/26` writes 3 vec4s per index; branch `0x1C0A81` uses the other vector combination path.

**Interpretation:** indexed model-subpart/bone transform palette (geometry/skinning), not LightBank physical row or material gain operator. This is a substantive **exclusion** when isolating camera-only armor flicker; it does not exclude spatial animation/deformation artifacts, but there is no actor/pixel link here. Do not patch the palette as an ambient/LightBank fix.

Original EXE verified with in-memory semantic mutation controls: V442 baseline **47 opcode+11 call+27 negatives**, independent material-context **46 opcode+7 call+18 negatives**, R2 palette **37 opcode+5 call+32 negatives**. Reproducible local source-only handoff `DSRRL_V442_R2_CONTEXT_AND_TRANSFORM_OPERATOR_RE_2026-11-06.zip`, sha256 `84a64ef632876eb455d00142f30b879f137a51aa6600252c077117bdba63503e`. GitHub source verifiers `verify_v442_context_multireceiver.py` and `verify_v442_indexed_transform_palette.py`, executed source snapshots in handoff. Original game EXE never included.

**Still OPEN:** direct GPU CB/SRV consumer of these blocks, actor generation and same source emitter → issued RX34 identity, real armor flicker root cause and PTDE pixel equivalence. V413/V414 row 30/31 switches known historically, not newly found.
