# V424 — PTDE type2/type5/type6 LightBank active consumers (static RE, 2026-10-31)

**Scope** original owner-provided PTDE DATA.exe PE32 SHA `88d1eec18eba2542c9c7e5e6903e89f9c075b20be5cd045ce6e568e7b22bea9d`, original DSR PE32+ SHA `a45aaa36dd2f6cc151670a639ea5547043cf38ea79ff4178b963c6ed71f98d7b`. No binary modification, no shader change, no new addon and no runtime pixel claim.

## First active PTDE bank routing beyond structural candidate V423

The PTDE standalone `D31590/D315B0` lookup helpers have zero direct relative calls; this did **NOT** prove the table is unused. Actual PTDE consumers **inline** the same area×type index:
- Manager area stride `0xD8`; table pointer `manager+area*0xD8+0x14`; type entry stride `8`, second 32-bit field `+4`. Area11 fallback table pointer `manager+0x95C` (`11*0xD8+0x14`).
- type2: PTDE `F92C62/F92CD9/F92D10`, table `+0x14=2*8+4`. `F92C00` has exact direct callsites `F5578A/F5CF42/F9876C`. It dispatches `FF1090` (single) or `FF1290` (blend), which calls `4BC590`, then `427260/427360`.
- **type5:** PTDE `F957BA/F95D1F`, table `+0x2C=5*8+4`, fallback area11/type5. These routes invoke `FF1A20` (e.g. `F957F4/F958A4`).
- **type6:** PTDE `F94CDA/F95248`, table `+0x34=6*8+4`, fallback to area11/type6 then area11/type5 `+0x2C`. Invoke `FF1A20` (e.g. `F94D1B/F94DCD`).
- Direct callers `FF1A20`: `D5AB86/D5B393/D5BAAF/F94D1B/F94DCD/F957F4/F958A4/FF1F39/FF1F5C`. The decoder calls `4BBC60` at `FF1A6D/FF1A8B/FF1AAA` for three 12-byte components.

## PTDE type5 record-format operator island candidate

PTDE `FF1A20` requires `WORD [block+8]==4`, row count `WORD [block+0xA]` and row table DWORD offsets `[block+0x34+12*row]`. It separately decodes three components at `record+0/+0xC/+0x18` through `4BBC60`, plus scaled color components. `4BBC60` converts two signed WORD angles using original double `0.01745329238474369` at `PTDE 0x1249740` (radians/degree), calls trigonometric routines `569310` and `569500`, and emits direction-like float4 (fourth component 0). It is NOT safely interchangeable with type2 record processing or scalar C86 gain. Receiver material class and exact DSR shader family remain unresolved.

## True record-layout and semantic cross-check — type2 ONLY

PTDE `FF1090` and DSR `565600` both use record block pointers PTDE `source+0x10` / DSR `source+0x18`, `row_count=WORD[block+0xA]`, row offset `DWORD[block+0x34+12*row]`, signed WORD fields `+0,+2,+6,+8,+A,+C,+E`. On valid rows, they produce two float4 vectors with symbolic operator
`A=[s0,s2-s0,0,s6*q]`,
`B=[s8*g/255,sA*g/255,sC*g/255,s6*q]`,
`g=sE*q`,
`q=float32(0.01)=0.009999999776482582`.
**Important:** PTDE uses double intermediates with float-rounding at selected steps; DSR uses predominantly float SSE with different operation order. Symbolic similarity is not numerical/pixel equivalence. DSR active type2 evidence: `1C8650 -> 569B40(area,type2)`, fallback `(11,2)`, `1C86F7 -> 565600`, then `1C8BB0/1C8B00`. PTDE inline type2 call graph verified independently.

## Important earlier false carrier: DSR context+0x1720 is resource metadata

`1EB320` cache miss constructs `float4(1,0,1,N)` for resource type2, `N=float(INT[resource+0x48])`; otherwise `float4(1,0,1,1)`. `1EB814 -> 1C0E40` copies this vector to `FrpgDrawContext+0x1720`. The cache-hit branch rebinds handles but jumps to `1EB85C`, skipping this vector rewrite. Thus `+0x1720` is **not proven C86 RGB**. `1C0E10` separately writes an index/flags DWORD plus 12 zero bytes to `+0x1740`. Both are CPU-side witnesses only. DSR native indices 11–14 still must not be assumed to equal D3D11 shader registers.

## RTTI provenance

MSVC RTTI `NS_FRPG::LightBankMan` exists in both: PTDE TypeDescriptor `0x013659F4`, COL RVA `0xE88620`, vtable VA `0x01204E88` with destructor VA `0x004BC570`; DSR TD VA `0x141B12E68`, COL RVA `0x16643F0`, vtable RVA `0x137D6C0`, destructor RVA `0x565540`. Class identity corroborates type-level operator comparison but does not prove receiver-level pixel mapping.

## Open before runtime / no patch

1. Identify DSR-specific homolog and exact equation of PTDE `FF1A20/4BBC60` type5 directional/SpecRGB parts by concrete shader/material family (including nonmetal).
2. Verify backend virtual `+0x40/+0x50` to actual D3D11 stage/resource and CB upload; CPU cache hit/miss is not GPU value equivalence.
3. Prove full native draw packet writer `56F4D0` instance identity to `2200D0` consumer.
4. Separate gameplay/cutscene, map, receiver, pass and HDR composition; PTDE pixel parity remains OPEN.

**Repro**: conversation handoff `DSRRL_V424_PTDE_TYPE5_STATIC_RE_HANDOFF_2026-10-31.zip` contains Python verifier, a *separate pinned instruction preimage manifest*, SHA manifest and detailed report, without proprietary game bytes.

**Statuses:** Original ABI, PTDE active type2/5/6 lookup and PTDE format4 direction decoder CONFIRMED static; type2 symbolic input equation HIGH CONFIDENCE; DSR type5 operator homology and all GPU/pixel behavior OPEN. Frozen V359-RC1 and R43 unchanged.