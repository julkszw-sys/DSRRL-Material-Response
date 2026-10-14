# V274D — PTDE original 24× HemEnvLerp SpecTex.a liveness and owner route audit

## Evidence carrier
Source **owner-uploaded** DSRRL_FINAL_EVIDENCE_PART02(2).zip exact archive SHA256 `57314b36812dfa2867512052278a08dc0da8a41ea0414c1fbbe4ccdf45838076`. Original PTDE FRPG_Phn_DifSpc and DifSpcBmp HemEnvLerp ps_3_0 shader files only; the binaries are **not redistributed** in GitHub. Reproducer `audit_original_ptde_ps30_alpha.py` accepts original ZIP externally; our local machine also generated a per-shader SHA report. No DSR EXE modifications.

## 24 exact shader source findings
- 24 complete original PTDE shader bytecode variants: 12 DifSpc and 12 DifSpcBmp across Csd/Sdw/plain and Lit/Mul variants. All 24 valid PS3.0, each CTAB has **sampler1 `gSMP_1`**.
- **24/24 sample the specular sampler s1 exactly once. 0/24 show a live read of the sampled specular texture alpha channel from its temporary register** under direct source-swizzle/destination-writemask dataflow analysis. Remaining sampled alpha registers in a few Mul variants are never read before shader END.
- **0/24 source IF/ELSE branches**, so no family-specific shader IF path makes sampled s1.alpha a live input in this enumerated group. Controlled mutation of the plain DifSpcBmp sample-following MUL src swizzle (original source token `0x80e40002` changed locally to `0x80ff0002`) produced exactly one alpha read; confirms the scanner can detect this particular unsafe alpha use. Mutation NOT shipped.
- This generalizes previous single-original-PTDE-shader source finding (Supabase rev12834) to the enumerated original 24 no-PointLight PTDE HemEnvLerp PS3.0 variants. It **does not establish** DSR GPU material routing into those exact PTDE homologs, nor visible pixel equivalence.
- Therefore the V272 R19 shader neutralization of DSR-only postmerge `SpecTex.a` is **not inherently evidence of a mistake** within this PTDE family. In particular one must not arbitrarily restore DSR SpecTex.alpha or a roughness multiplier for all Cloth/Leather as a laminate fix.

## Owner V274C beta witness material identities: exclude wrong Cloth assumptions
Join actual owner log `V203 OWNER TRACE raw4` with canonical exact raw SHA:
- Route **160**: P_DullLeather[DSB] (`7ea7d6ce`) 39 bounded sampled prepared CPU shadows.
- Route **88**: P_DullLeather[DSB]_Edge (`44e1188b`) 12.
- Route **1073741869**: ordinary P_Leather[DSB] (`0f2b9a10`) 9.
- Route **232**: **Ps_Body[DSB] (`af2f1088`)**, 19. Do NOT count as Leather/Cloth merely because prior generic logs say `class=NONMETAL`.
- Route **345**: exact P_Metal 17, control only.
All owner shadows are prepared CPU producer, `flver=0` incomplete physical identity, actual GPU execution/pixel Open. **No exact RoughCloth/C_RoughCloth MTD was captured in these 96 bounded rows**. Any statement that current V274C logs empirically establish an incorrect Cloth texture or cloth gloss root would be false.

## Material-by-material candidate frontier
- Exact P/C_RoughCloth and Edge records: 4 direct MR table entries, PTDE c101=1, c102=2, env slot0; authentic CURRENT material-class draw identity, native s1 alpha and preFog RGB **unseen**. Do not derive cloth appearance from leather samples. Earlier Supabase rev12002 source confirms generic MR is not complete Cloth SpecRGB response.
- P_Leather[DSB] shared-base extension requires PTDE companion; P_Leather[DS] PTDE/DSR SPX mismatch; four P_Damage L/S Dull/Leather records need semantic name+FLVER/slot due identical DSR MTD hashes with unequal PTDE c101/c102.
- C_DullLeather Lit records use RX30..32, not RX33..35 R19. Any Cloth/Leather blanket R19 shader-family fix misses them.
- C_Fire_DullLeather and Edge c101=3 is authored PTDE target, not presumed excessive specular gain.

## Status
- SOURCE: 24/24 alpha-not-consumed CONFIRMED as a local source-dataflow scan; binary SHA per owner source file retained in local JSON output.
- MATERIAL: 27 source-route MTD audited via V274D 37,665 isolated synthetic beta canaries; **no local endpoint overshoot**. This is not full shader/pixel simulation.
- RUNTIME: sample contains 3 ordinary leather/dull route families and body/metal control, **zero cloth**.
- FIRST MISMATCH / PIXEL: OPEN. Next need exact live material/FLVER/slot/SRV, compare R19 preFog, after Fog, after LS, after postgamma and PTDE at matching camera/texel/probe. Preserve V272 and stock independent host systems.
