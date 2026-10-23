# PTDE 2026-10-10 1295 SpecRGB: original FLVER source reconciliation (2026-10-15)

Status: **CONFIRMED original PTDE source ownership**, **OPEN DSR cross-version runtime routing/atomic consumer/pixel behavior**.

Source is the owner's retained native Library object:
`/DSRRL/source-corpora/DSRRL_PTDE_FLVER_CENSUS_FIXED.zip`
- SHA256: `6651acb4fe995a69f1c84e5761b3d6bdcadeb39e1fc4d1087c3b82794f4d7281`.
- Exact raw per-texture JSONL: `flver_material_ownership_raw_v1.jsonl`, SHA256 `c762b9a4705ed422e13d439b1a6ccf01a1778decaa5c8526dd85b3d27027eb37`.
- 87,151 original PTDE texture-binding rows. The full SHA-256 of each PTDE FLVER, material-slot index, MTD basename and raw SHA256, original `g_Specular` texture path and source archive chain are embedded in the JSONL.
- Oct10 names SHA256 `6f417110f6085ca8389213578eb9c0a86c100070717a9df20d62790052c151e4`.

**Result of exact original texture-path basename join (no alias):**
- All **544/544** previously unresolved `*_l_s` names each have original PTDE `g_Specular` FLVER binding: **1,796 exact SHA256+slot+MTD+path records**, across 20 MTDs.
- All **749** names directly used in the owner DSR parts scanner also have original PTDE `g_Specular` FLVER bindings.
- **1,293/1,295** October logical SpecRGB names have a precise PTDE FLVER material ownership record; 6,444 original PTDE `g_Specular` rows.
- Only `wp_a_0911_s` and `wp_a_1004_s` lack direct material `g_Specular` bindings in original PTDE FLVER corpus (not proof that the textures do not exist in TPF/asset banks).
- `AM_A_1000_L_s` is original PTDE `AM_A_1000_L.flver` `P_Metal[DSB].mtd` slots; `HD_A_3460_L_s` is original PTDE `HD_A_3460_L.flver` with five distinct material slots. Low variants **are not guessed aliases** of base `_s`.

Full derived CSV output: `DSRRL_PTDE_EXACT_544_FLVER_SLOT_MTD_RECOVERED_20261015.zip` in conversation artifacts, including all 1295 name statuses and the 1796 exact original low owner/slot/MTD rows.

This exact source evidence supersedes the prior 261-MTD projection's statement of only 543/544 positive families. **Never** use source PTDE FLVER SHA as a DSR FLVER SHA. Runtime `t1` companion identity, DSR material owner bridge, shader family, CB and atomic PTDE operator still require independent verification. No blanket material gate and no new addon is authorized by this source join alone.
