# V428 | Confirmed original DSR type5 FrpgDrawContext -> D3D11 PS b0 upload (static RE)
Date: 2026-10-31. Original DSR SHA256 `a45aaa36dd2f6cc151670a639ea5547043cf38ea79ff4178b963c6ed71f98d7b`. Original PTDE DATA.exe SHA256 `88d1eec18eba2542c9c7e5e6903e89f9c075b20be5cd045ce6e568e7b22bea9d`. No game EXE modification, no new addon or runtime.

**New exact DSR native dataflow** (attested original instruction preimages):
- Type5 native `1C6900` invokes `563B80` (raw type5 record+0x40/+0x48 decode via `564510` abs→powf 2.2) at `1C69FE`; writes float4 vectors to `FrpgDrawContext+1240,+1250,+1260,+1270` and expanded values to `+1280..+12B0`.
- Native uploader `1E08EF..1E0959` checks valid `[ctx+210]` and `[guard+18]`; reads `count=DWORD[ctx+2230]` at `1E0902` and `src=ctx+1230` at `1E0909`. Selects wrapper `manager+2B0+8*idx` (capacity category 4..7), calls `CD0A10` at `1E0931`, extracts ID3D11Buffer* from wrapper+28, and uses context vtable+80 with `StartSlot=0, NumBuffers=1`: **PSSetConstantBuffers(0,1,&buffer)** at `1E0959`. It is PS **b0**, not PS SRV slot t11..t14 (separate V427 resource/sampler binders).
- `CD0A10` calls ID3D11DeviceContext::Map vtable+70 with `D3D11_MAP_WRITE_DISCARD=4`, copies exactly `count*16` bytes by `movaps xmm0,[source]` / `movaps [dest],xmm0`, advances each pointer by 16, Unmap via vtable+78. Zero count skips Map; the outer code still binds the buffer. **No color/gamma conversion in this uploader.**
- VS sibling `1E0888..1E08E5`: VS source `ctx+220`, count `ctx+1220`, same uploader, vtable+38 VSSetConstantBuffers b0. Separate native path `064A20 -> 064900` independently reads PS count `ctx+2230`, source `ctx+1230`, calls `CD0A10` and binds PS b0 via +80 (`06496D`).

**Conditional PS b0 layout**: `ctx+1240` -> float4 index c1, `+1250` c2, `+1260` c3, `+1270` c4, `+1280` c5, `+1290` c6, `+12A0` c7, `+12B0` c8, provided active upload count covers each index (at least 2..9 vectors respectively). These are **physical CB byte positions, not proof that target RX34 shader reads them**, nor that PTDE c-register indices are homologous.

**PTDE**: previous V427 D3D9 PSSetPixelShaderConstantF uploader `431870 -> 431760` verified. LightBank type5 CPU writer `F94260` gets backend pointer B=[writer+10], mask B+1050, payload B+15F0, marks logical index 0x56. Uploader uses state S+11D0; actual pointer alias B=S+180 has NOT been proven, so a PTDE PS c86 shader link is OPEN.

**Evidence and verifier**: conversation handoff `DSRRL_V428_TYPE5_PS_B0_GPU_UPLOAD_STATIC_RE_HANDOFF_2026-10-31.zip` has complete report + standalone original-binary SHA-locked Python verifier: 50 DSR and 8 PTDE byte-exact preimages, 8 direct call edges, result JSON. Test PASS. *Do not use an EXE-modifying hook.* Local owner binaries omitted.

**Next static priority**: identify DSR compiled PS shader/receiver and actual CB b0 c1..c8 consumption (gamma, beta LERP, saturation, HDR), PTDE native writer-to-D3D9 instance identity and shader; classify homologous raw asset pairs and check R43 protections. No global gamma undo, row pinning, or bridge yet.

Stages: STATIC NATIVE ABI CONFIRMED; runtime vector count OPEN; shader/receiver mapping OPEN; bridge activation OPEN; pixel OPEN. Frozen V359-RC1 and R43 unaffected.