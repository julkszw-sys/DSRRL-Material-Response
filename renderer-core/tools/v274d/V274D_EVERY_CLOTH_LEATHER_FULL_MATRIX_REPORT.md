# V274D — every Cloth and Leather V272 HemEnvLerp source/audit-simulation matrix (2026-10-14)

**Scope / authority**: exact frozen owner V272 commit `ae9ac314d06ad8758e1aa7430cb236eef0eefe27`; PTDE/DSR exact-mapped MTD router 325 records, of which **27** names contain Leather or Cloth. Full MTD hash is retained in the generated CSV, table below shows short identification only. Matched direct Material Response route/extension by **complete raw SHA** (not name-only). Source-and-construction audit, **not** PTDE pixel verification or GPU complete shader execution. Read with Supabase PROTECT.

## Reproducible numerical sweep

Native GitHub Actions **38645003823 SUCCESS**, 27 records × 5 synthetic SpecRGB samples × 3 synthetic COLOR0 values × 3 synthetic A/B scalar environment-probe pairs × 31 beta samples = **37,665 operator-local cases**. Implemented in `audit_cloth_leather_lerp.py`, generated artifact `v274d-source-material-matrix-and-beta-sweep` ID **11932775135**.

For the **isolated documented** R19 subcut only:
`S(beta) = saturate(SpecTexPTDE.rgb * c101 * COLOR0.rgb * ((1-beta)*EnvSpecA + beta*EnvSpecB))`.
Any fixed nonnegative texel / COLOR0, beta in [0,1] and nonnegative fixed endpoints satisfy `min(S(0),S(1)) <= S(beta) <= max(S(0),S(1))`. Observed **0/37,665 test overshoots**. This is algebraically expected even with local terminal SAT; cannot explain transient laminate by **that scalar local product alone**. Does NOT prove whole PHN or PTDE/DSR final pixel: actual SpecRGB DDS texels/alpha, per-pixel normal/basis, PTDE probe filtering, secondary MaterialWorkflow branch, Fog/LightScattering transfer, GPU preFog and postFog outputs missing. Numeric inputs are intentionally synthetic, never described as captured game textures.

## Complete 27-record census

| Exact semantic MTD name | Raw DSR SHA prefix | PTDE c101 | PTDE SpecPower c102 | PTDE slot | Route classification | Specific flags |
|---|---|---:|---:|---:|---|---|
| /C/_/5/2/5/0/_/F/i/r/e/_/D/u/l/l/L/e/a/t/h/e/r/[/D/S/B/]/./m/t/d/ | `23dbc2903320` | 0.5 | 2 | 0 | unregistered in two tables | no direct MR/extension |
| /C/_/5/2/6/0/_/L/e/a/t/h/e/r/[/D/S/B/]/./m/t/d/ | `0c27e030221f` | 1.5 | 4 | 0 | unregistered in two tables | no direct MR/extension |
| /C/_/D/u/l/l/L/e/a/t/h/e/r/[/D/S/B/]/./m/t/d/ | `613a3296e790` | 1 | 2 | 0 | extension #46 (PTDE_COMPANION_REQUIRED) | sidecar required |
| /C/_/D/u/l/l/L/e/a/t/h/e/r/[/D/S/B/]/[/L/]/./m/t/d/ | `6352d64c658a` | 1 | 2 | 0 | direct #10 | Lit family RX30–32 |
| /C/_/D/u/l/l/L/e/a/t/h/e/r/[/D/S/B/]/[/L/]/_/A/l/p/./m/t/d/ | `af64a2f6fa37` | 1 | 2 | 0 | direct #11 | Lit family RX30–32 |
| /C/_/D/u/l/l/L/e/a/t/h/e/r/[/D/S/B/]/_/A/l/p/./m/t/d/ | `c1b1baca69cc` | 1 | 2 | 0 | direct #276 | no listed static route warning |
| /C/_/D/u/l/l/L/e/a/t/h/e/r/[/D/S/B/]/_/E/d/g/e/./m/t/d/ | `e2d819b90e82` | 1 | 2 | 0 | direct #331 | no listed static route warning |
| /C/_/F/i/r/e/_/D/u/l/l/L/e/a/t/h/e/r/[/D/S/B/]/./m/t/d/ | `29318e9f36bc` | 3 | 2 | 0 | unregistered in two tables | no direct MR/extension; high PTDE c101 by design |
| /C/_/F/i/r/e/_/D/u/l/l/L/e/a/t/h/e/r/[/D/S/B/]/_/E/d/g/e/./m/t/d/ | `d2b73d50ad2d` | 3 | 2 | 0 | unregistered in two tables | no direct MR/extension; high PTDE c101 by design |
| /C/_/L/e/a/t/h/e/r/[/D/S/B/]/./m/t/d/ | `236bb0a0614e` | 1.5 | 4 | 1 | direct #41 | no listed static route warning |
| /C/_/L/e/a/t/h/e/r/[/D/S/B/]/_/A/l/p/./m/t/d/ | `9ef69c874a7c` | 1.5 | 4 | 1 | direct #12 | no listed static route warning |
| /C/_/L/e/a/t/h/e/r/[/D/S/B/]/_/E/d/g/e/./m/t/d/ | `25ffd8635203` | 1.5 | 4 | 1 | direct #44 | no listed static route warning |
| /C/_/R/o/u/g/h/C/l/o/t/h/[/D/S/B/]/./m/t/d/ | `9885c34b18f2` | 1 | 2 | 0 | direct #14 | no listed static route warning |
| /C/_/R/o/u/g/h/C/l/o/t/h/[/D/S/B/]/_/E/d/g/e/./m/t/d/ | `0a29732f36dc` | 1 | 2 | 0 | direct #15 | no listed static route warning |
| /P/_/D/a/m/a/g/e/_/L/_/D/u/l/l/L/e/a/t/h/e/r/[/D/S/B/]/./m/t/d/ | `0f8ca2b6896c` | 0.25 | 0.5 | 0 | unregistered in two tables | collision SHA / different c101; no direct MR/extension |
| /P/_/D/a/m/a/g/e/_/L/_/L/e/a/t/h/e/r/[/D/S/B/]/./m/t/d/ | `0f8ca2b6896c` | 0.5 | 1.25 | 0 | unregistered in two tables | collision SHA / different c101; no direct MR/extension |
| /P/_/D/a/m/a/g/e/_/S/_/D/u/l/l/L/e/a/t/h/e/r/[/D/S/B/]/./m/t/d/ | `c28d10119202` | 0.5 | 1 | 0 | unregistered in two tables | collision SHA / different c101; no direct MR/extension |
| /P/_/D/a/m/a/g/e/_/S/_/L/e/a/t/h/e/r/[/D/S/B/]/./m/t/d/ | `c28d10119202` | 1 | 2.5 | 0 | unregistered in two tables | collision SHA / different c101; no direct MR/extension |
| /P/_/D/u/l/l/L/e/a/t/h/e/r/[/D/S/B/]/./m/t/d/ | `7ea7d6ce9bb7` | 1 | 2 | 0 | direct #160 | two raw SHA same name |
| /P/_/D/u/l/l/L/e/a/t/h/e/r/[/D/S/B/]/./m/t/d/ | `9376d84c81f0` | 1 | 2 | 0 | unregistered in two tables | no direct MR/extension; two raw SHA same name |
| /P/_/D/u/l/l/L/e/a/t/h/e/r/[/D/S/B/]/_/E/d/g/e/./m/t/d/ | `44e1188bafa9` | 1 | 2 | 0 | direct #88 | no listed static route warning |
| /P/_/L/e/a/t/h/e/r/[/D/S/B/]/./m/t/d/ | `0f2b9a1012b8` | 1.5 | 4 | 1 | extension #45 (PTDE_COMPANION_REQUIRED) | sidecar required |
| /P/_/L/e/a/t/h/e/r/[/D/S/B/]/_/A/l/p/./m/t/d/ | `4c728a9b5957` | 1.5 | 4 | 1 | direct #0 | no listed static route warning |
| /P/_/L/e/a/t/h/e/r/[/D/S/B/]/_/E/d/g/e/./m/t/d/ | `cf968a227226` | 1.5 | 4 | 1 | direct #1 | no listed static route warning |
| /P/_/L/e/a/t/h/e/r/[/D/S/]/./m/t/d/ | `53819ead337c` | 1.5 | 4 | 1 | extension #44 (DIRECT_EXACT) | PTDE/DSR SPX difference |
| /P/_/R/o/u/g/h/C/l/o/t/h/[/D/S/B/]/./m/t/d/ | `db197a28abd9` | 1 | 2 | 0 | direct #6 | no listed static route warning |
| /P/_/R/o/u/g/h/C/l/o/t/h/[/D/S/B/]/_/E/d/g/e/./m/t/d/ | `4b3451c1d83d` | 1 | 2 | 0 | direct #7 | no listed static route warning |

## Proven high-value static anomalies

1. **Two conflicting same-SHA semantic clusters:** `P_Damage_L_DullLeather` c101=.25 p=.5 versus `P_Damage_L_Leather` c101=.5 p=1.25 share DSR raw SHA prefix `0f8ca2b6896c`; `P_Damage_S_DullLeather` c101=.5 p=1 versus `P_Damage_S_Leather` c101=1 p=2.5 share `c28d10119202`. MTD-SHA-only routing cannot select the correct PTDE semantic response. Requires **exact FLVER+slot+semantic name/texture identity**. These entries do NOT have a matching base MR/extension record; this prevents asserting current corruption, but demonstrates an unsafe expansion policy.
2. **Different shader family:** `P_Leather[DS].mtd` PTDE `FRPG_Phn_ColDifSpc.spx` versus DSR `FRPG_Phn_ColDifSpcBmp.spx`; special exact-binding extension #44 does not establish shader operator homology or authorize arbitrary SpecRGB Bmp bridge.
3. **Two Lit-only records** `C_DullLeather[DSB][L]` and `..._Alp` route #10/#11 have `SpcBmpLit` rather than the owner materialized R19 RX33/34/35. Treat separately; do not claim RX33 shader fixes their liveout.
4. **Authoring / routing gaps:** 15/27 records have a direct entry in the MR route dataset, 3 use exact-binding extension (#44/45/46, with #45/46 PTDE companion required), **9 have neither of these registration forms**. Neither table alone enumerates all possible V272 EnvSpec runtime bypasses; this is a source matrix gap, NOT proof they never render or that they receive wrong coefficients. Physical owner V274C 96 shadow samples include P_Leather route1073741869 but no identified Cloth route, and have no complete FLVER SHA.
5. **Same semantic name with two verified raw SHAs:** P_DullLeather[DSB] `7ea7d6ce9bb7` direct #160 and `9376d84c81f0` absent in two route datasets. Both c101=1 p=2 but SHA and route must not collapse. `C_Fire_DullLeather[DSB]` and its Edge use **PTDE c101=3** and p=2, not ordinary c101=1: high authored response is **not evidence of a bug**, but a blanket 'cloth/dull' fixed scalar would corrupt those two.
6. **R19 MaterialWorkflow ALPHA cut is source-real:** zero MaterialWorkflow IF branch R15 removes additive DSR PBL; R16 removes alpha/workflow diffuse suppression; R17 removes cb9->cb10 workflow mix; late SpecTex.a gate is neutralized. Separately PTDE t10 SpecRGB*c101*COLOR0 path is added. V274B/V274C owner CPU shadow cb85=0 96/96; GPU consumption OPEN. Original PTDE DifSpcBmp HemEnvLerp DX9 sample alpha is not used before register overwrite (Supabase rev12834), thus removal alone cannot be assumed the nonmetal laminate root. The earlier V273 wholesale stock-MaterialWorkflow visual fix was REJECTED by owner.

## Current owner log constraints

V274C 96 prepared CPU shadow records: RX33 74 RX34 22; route counts `160=39, 232=19, 1073741869=9 (ordinary leather), 345=17 (metal control), 88=12`; five sampled beta states. cb85=0, cb100.w=1, cb102=0 in captured producer, identical cb12..20 across materials at same beta. **No Cloth exact draw is identified in that sample.** Four V250 P_Metal batch/issued records (two issue, two batch), not material-wide PTDE execution. Every sampled FLVER physical hash zero; actual material-sidecar logical name and GPU preFog/postLS final RGB unverified.

## Next operator-local investigation

Before authoring any corrective bridge, perform receiver/material/slot exact match and preFog post-specRGB/liveout inspection for `P_RoughCloth`, `C_RoughCloth`, `P_Leather`, `C_Leather`, `P_DullLeather`, including BOTH variants, and author-class exceptions above. Compare same physical material, texture coordinates, camera and LightBank A/B in Stable A, Lerp beta0/interior/beta1, Stable B. First mismatch stage (material, SpecRGB, EnvSpec, PHN, Fog, LS, postgamma) required; no arbitrary gain or global atmosphere tweak. V272 frozen unchanged.

**STATUS:** Census CONFIRMED; static route gaps/collisions CONFIRMED; 37,665 bounded local numeric operator tests PASS; *whole R19 shader, actual texture/receiver draw and PTDE pixel equivalence OPEN*.
