# V427 — PTDE D3D9 PS constants and DSR D3D11 PS resource/sampler GPU binding (static RE)

2026-10-31. Original retail PTDE DATA.exe SHA256 `88d1eec18eba2542c9c7e5e6903e89f9c075b20be5cd045ce6e568e7b22bea9d` and DSR EXE SHA256 `a45aaa36dd2f6cc151670a639ea5547043cf38ea79ff4178b963c6ed71f98d7b`. No executable mutation, new addon or runtime.

## New confirmed PTDE D3D9 uploader cut
- PTDE additional type5 light color: F94260: B=[writer+0x10], dirty base M=B+0x1050, logical index 0x56 (86), vector M+0x5A0=B+0x15F0. F94380 uses same layout for directions 0x5C..0x5E (B+0x1650..) and colors 0x5F..0x61 (B+0x1680..). Formula: M+0x40+16*index.
- PTDE 431870 dispatches 431650 (VS) and 431760 (PS); 431760 scans two groups of 8 x DWORD bitmasks and clears dispatched bits, coalesces runs, calls IDirect3DDevice9::SetPixelShaderConstantF via device vtable +0x1B4 with StartRegister, address of buffer +0x40+16*register, Vector4fCount. 431650 analogously calls SetVertexShaderConstantF via +0x178.
- PS upload block base is S+0x11D0, writer base M=B+0x1050. Equality is possible if B=S+0x180. **No static per-instance proof of B=S+0x180 has been obtained.** Therefore type5 writer register 0x56 -> PS c86 is HIGH CONFIDENCE *candidate*, NOT established pointer routing; likewise directional c92..94 and RGB c95..97. PS stage uploader and its own register mapping are confirmed.

## New DSR D3D11 PS resource/sampler API cut
- DSR 0BAD60 obtains SRV from resource wrapper +0x30, passes slot in RDX, 1 in R8D and &SRV in R9 to [device-context vtable+0x40] through binding state+0x20; matches ID3D11DeviceContext::PSSetShaderResources.
- DSR 1EAC30 obtains sampler from wrapper +0x28 and passes same arguments to [device-context vtable+0x50]; matches ID3D11DeviceContext::PSSetSamplers.
- Original type5 1EB320 *cache hit* uses resource handles ctx+0x2354/0x2358 at slots t11/t12 and samplers s11/s12. *Cache miss* resolves new bank records through 569B40/563F90/564000/1EAD10 and binds slots 11/13 for first source, 12/14 for second source.
- These are physical PS resource and sampler slots for this native operator, NOT LightBank RGB constant registers. D3D11 interface type provenance back to GetImmediateContext remains OPEN (vtable ABI and arguments confirmed). Do not generalize to other receivers or count this as a pixel result.

## Open
- PTDE writer B -> D3D9 uploader S pointer identity.
- DSR type5 CPU RGB FrpgDrawContext+0x1240..0x12B0 -> exact D3D11 CB, buffer ABI, shader receiver. No direct offset read is not evidence of no indirect upload.
- Material/MTD family, logical assets, gamma-domain downstream and final PTDE pixel. Protect R43 Source2 and frozen V359-RC1.

## Reproduction
Independent SHA-pinned verifier (33 instruction preimages +16 exact CALL rel32) PASS on original owner-provided executables. Conversation artifact `DSRRL_V427_GPU_BINDING_STATIC_RE_FULL_HANDOFF_2026-10-31.zip` includes full detailed report, verifier, JSON and integrity manifest. Repo source test `renderer-core/tools/verify_v427_gpu_abi_smoke.py` rechecks critical cut.

Statuses: STATIC ABI PASS, PTDE PS API CONFIRMED, DSR PS SRV/SAMPLER interface ABI HIGH CONFIDENCE, pointer alias/CB/shader/PIXEL OPEN. No active addon changes.