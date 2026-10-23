# V295 PTDE *_L to DSR base models — exact source-level comparison (2026-10-15)

**Scope: offline FLVER material topology. No new atomic island or runtime/pixel claim.**

## Inputs (SHA-256)
- PTDE corrected FLVER 87,151-row census: `6651acb4fe995a69f1c84e5761b3d6bdcadeb39e1fc4d1087c3b82794f4d7281` (owner Library `/DSRRL/source-corpora/DSRRL_PTDE_FLVER_CENSUS_FIXED.zip`).
- DSR FLVER structural prefix corpus retaining original FULL FLVER hashes: `a9a2e0eb48625fc735dfe18f7f375105cc5f56be005022969300c778489d0891` (`DSR_FLVER_CENSUS_COMPACT.zip`, owner Library `/DSRRL/source-corpora`).
- Owner independent DSR parts source scan V1.1 `DSRRL_PARTS_MAPPING_RESULT(1).zip`.
- Original Oct10 1,295-name resource manifest SHA `6f417110f6085ca8389213578eb9c0a86c100070717a9df20d62790052c151e4`.

## Source finding
- 544 unresolved Oct10 `_l_s` names have 1,796 direct PTDE `g_Specular` SHA+slot+MTD bindings across **451** PTDE source models.
- **449** PTDE sources are separate `*_L.flver` models; **0/449** low-basename files exist in DSR full structural corpus, while **449/449** corresponding base `*.flver` files do.
- Remaining two PTDE source models are `c5300.flver` (chr) and `o9993.flver` (obj), both remain in DSR, but their special 14 PTDE low texture references do not retain the same `g_Specular` in corresponding DSR chr/obj model.
- **1,761/1,796** PTDE material source rows correspond to DSR base model **with exactly the same material slot and MTD basename**, where `g_Specular` logical name becomes the PTDE name without `_L`. These alignments touch **541 of 544** logical names.
- **1,761/1,761** DSR sha+slot+MTD alignments independently occur in the owner's separate V1.1 original binder scan.
- **21** additional PTDE source material rows have base-model `g_Specular` counterpart but a **different MTD** in DSR, including `hd_a_3460_l_s` using DSR `*_Spec` MTDs and `hd_f_9740_hd_m_9740_l_s` / `hd_m_9740_l_s` where PTDE `C_Leather[DSB]` becomes DSR `P_Leather[DSB]`. Other source rows with different MTD for a name may co-exist with source rows of the same logical name that are aligned.
- **14** original PTDE rows use source `c5300`/`o9993` with changed DSR source-model `g_Specular` (different draw ownership). Do not infer equipment identity of chr/obj from shared logical names.
- Channel proof across 1,761 aligned source pairs: `g_Diffuse` **1761/1761**, `g_Specular` **1761/1761**, `g_Bumpmap` **1758/1761** after removing `_L` suffix in original texture name. For the remaining three DSR has additional normal map bound; `g_DetailBumpmap` **1761/1761**. This is source name topology only, not equal pixels/texels.
- Independent V290 one-session runtime sample: 649 draw census rows, 630 resolved **base** SpecRGB names, 19 no logical name; **0** original `_l_s` observed. This does not establish global reachability absence.

## Routing consequence
Stock DSR generally routes `*_s` through BASE FLVER, not PTDE's separate `*_L.flver` owner. **Never** authorize 544 low sidecars through a blanket base alias or material-family gate: PTDE low texture is a separate resource, and corresponding source draw/LOD selection is not proven. Narrowest verified next cut is actual DSR model/LOD receiver material and PTDE source LOD selection. If source PTDE geometry/LOD differs, exact asset/LOD island is required before pixel-equivalence, rather than pushing `_L_s` into stock base `t1`.

Separate real outliers: `hd_a_3460_l_s`, `hd_f_9740_hd_m_9740_l_s`, `hd_m_9740_l_s`. 35 source rows need separate treatment (21 changed MTD + 14 chr/obj).

Full CSV materialization and reproducer: `DSRRL_V295_PTDE_DSR_EXACT_LOW_LOD_COUNTERPART_20261015.zip` (conversation artifact) SHA-256 `cc6b642ffe40a0acfbba29ce5400608ef40b76b13622600275702acaf3b3d6ae`. Includes source script and per-channel proof. No addon source/binary modified in this evidence-only commit.
