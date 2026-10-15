# V279 follow-up: actual Stable-only R8 specular pow(2.2) cut, Lerp missing

**Sources:** Owner V277 private exact twelve original D3D11 DXBC captures (SHA archive `fcb182f4569229b8985743d1068e89b198723d3b2d97b4200437ad6298605981`) and owner V278 Metal GOOD/Leather BAD report. RX33, RX34, RX35 all independently checked. Original game shader bytes must remain private.

## Exact instruction census in b0[85] ELSE (three receivers, each four variants)

| Shader | RX33 | RX34 | RX35 |
|---|---:|---:|---:|
| Stable STOCK: exact LOG→MUL(2.2)→EXP | 1 | 1 | 1 |
| Stable BRIDGE: exact LOG→MUL(2.2)→EXP | **0** | **0** | **0** |
| Lerp STOCK: exact LOG→MUL(2.2)→EXP | 1 | 1 | 1 |
| Lerp BRIDGE: exact LOG→MUL(2.2)→EXP | **1** | **1** | **1** |

The opcode tokens are exactly LOG `0x2f` length5, MUL `0x38` length10 with XYZ literals `0x400ccccd,0x400ccccd,0x400ccccd`, EXP `0x19` length5. **Unique** in the `IF b0[85].x / ELSE` region for each case, and all three receivers match.

The frozen V272 Stable shader materializer has explicit R8 source `apply_pmetal_pre_ab` (V2.28A), which **NOPS LOG and MUL(2.2), converts EXP into MOV** before the diffuse/EnvSpec final tail, removing the stock DSR SPEC-domain transfer. The Lerp materializer lacks the corresponding named R8 cut and the final assembled DXBC **still contains** the original LOG/MUL/EXP triple. Unlike prior R5 hypothesis, this discrepancy lives in a **material-response/specular-domain operator**, where visibly GOOD metal and BAD leather can respond differently to the same formula.

Original owner PTDE 24 no-PointLight Stable/Lerp PS3 pairs contain zero literal 32-bit `0x400ccccd` tokens in both phases. This supports a source-level target contrast but does **not** prove no PTDE 2.2 can arrive through constants, or that GPU executes the exact DSR ELSE branch.

**Guards:**
- CPU shadow b0[85]=0 in owner log is not GPU register readback. GPU branch liveness OPEN.
- The relative specular-domain operator is verified at shader-construction level, but the first wrong GPU live-out, PTDE pixel target and material-specific visual cause remain OPEN.
- V279 read-only material source carrier snapshot should check exact CB/SRV/sampler owners for GOOD metal/BAD leather before any conditional shader patch.
- If physical GPU and PTDE evidence confirm this second gamma transfer as the first mismatch on BAD leather, choose exact guarded Lerp R8-only operator island; do not globally disable PBL or specular, and independently verify correct Metal remains correct.
- Blindly restoring stock DSR MaterialWorkflow is *not* a substitute: V273 has already been tried and had negative pixel result / metal regression.

**Status:** SOURCE R8 DISCREPANCY CONFIRMED all three RX; R8 as cause HYPOTHESIS; GPU/PTDE pixel OPEN; no renderer gamma edit made by V279.
