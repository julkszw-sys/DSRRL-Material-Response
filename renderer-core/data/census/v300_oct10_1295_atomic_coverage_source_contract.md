# V300 sidecar atomic source expansion (October 16, 2026)

Scope: exact PTDE-visible material operator, DSR renderer as host. Original PTDE full FLVER source ZIP SHA256 6651acb4fe995a69f1c84e5761b3d6bdcadeb39e1fc4d1087c3b82794f4d7281.

October 1295 SpecRGB names:
- 1293 have original PTDE FLVER g_Specular ownership.
- 749 names have direct stock DSR g_Specular references across 4455 exact FLVER SHA+slot+MTD rows.
- 544 are separate PTDE low LOD _L_s names, lacking direct DSR g_Specular _L_s references. They are NOT proven aliases.
- wp_a_0911_s and wp_a_1004_s lack original FLVER g_Specular ownership in both source censuses.

Operator cuts and versions:
V296: fails open for Oct10 stock t1 material without complete PTDE consumer, including legacy inactive MR; independently certified operators preserved.
V297: exact PTDE DullLeather / Edge / Ps_Body slot0 original donor source, raw MTD and complete owner tuple; no guessed shader.
V298: actual retail parser raw-byte SHA256 observer, joined to exact material pointer + source FLVER SHA/slot/MTD.
V299: source LightBank and atomic draw consumer connected for V297 slot0 materials; requires exact RX33..35.
V300: independently original PTDE SPX FRPG_Phn_ColDifSpcBmp.spx verified for P_Leather and P_Leather_Edge, original authored EnvSpec slot1, c100 RGB=0.5, c101=1.5, specular power=4.0, DSR raw-MTD full SHA, source owner, actual parser bytes and RX33..35.

Static source coverage: V299 MTD rows 1987 (407 unique logical names), V300 MTD rows 793 (248 unique names), potentially overlapping. They are NOT runtime atomic or pixel PASS counts.

The V300 recipe renderer-core/tools/build_v300_one_atomic_addon.ps1 combines all cuts into one opt-in addon. GitHub Win64 PR job v300-leather-atomic-pr tests its native construction.

RUNTIME STATE OPEN: full native compile, addon liveness, hook identity, receiver activation, sidecar resource hit, complete shader/CB/texture transaction, and PTDE pixel comparison must all pass independently.
The entire 1295 atomic target is NOT YET VERIFIED. Notably 544 separate PTDE low-LOD sources require correct asset/LOD operator islands, and two weapons require a verified source identity.
