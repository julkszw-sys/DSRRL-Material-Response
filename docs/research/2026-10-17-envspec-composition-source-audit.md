# EnvSpec composition — PTDE vs DSR / V336 source audit (2026-10-17)

**Scope:** Material/EnvSpec composition only. Do not infer cubemap/LOD/probe, PointLight, runtime pixel equivalence. Exact V336 parent: `32c894c0df8e1ffd6beff964fe5e598a0dbcd989`.

## Original PTDE ps_3_0 source (RX33 Csd / RX34 Sdw / RX35 plain)

Original source: user-provided `DSRRL_FINAL_EVIDENCE_PART02(2).zip`, `PTDE/PTDE_FRPG_PHN_EXTRACTED/dvdbnd1_dcx_0000631D9390/FRPG_Phn_DifSpcBmp*HemEnv[ Lerp].fpo`.

At the EnvSpec consumer:

```
A1 = sample(s12).rgb / sample(s12).a * PTDE_c87.xyz
B1 = sample(s14).rgb / sample(s14).a * PTDE_c85.xyz
E_lerp = A1 + PTDE_c85.w * (B1 - A1)
E_stable = A1
Spec = geometry_factor * E * SpecRGB_s1.rgb * c101.rgb * COLOR0.rgb
Diffuse = (TexDiffuse_s0.rgb + c156.rgb) * c100.rgb * COLOR0.rgb * (EnvDiffuse_AB + hemispherical contributions)
C_surface = Spec + Diffuse
C_pixel = SAT(PTDE_scene_encoding(C_surface, ...))
```

PTDE RX33 Lerp original bytecode: s14/s12 samples at DWORD 1189/1193, RGB/alpha at 1197–1207, c87/c85 terms at 1211/1215, beta at 1220, geometry at 1225, SpecRGB/c101/COLOR0 at 1229/1233/1237, diffuse+spec MAD at 1261, terminal SAT|PP at 1376. RX34/35 have the same operation pattern. Each original PTDE Stable/Lerp pair ends in SAT|PP (dest token 0x80370800). PTDE c85 is a PS constant, NOT DSR cb0[85].

## Original DSR DXBC source

From the same user archive `DSR/shader/FRPG_FlverPBL_fpo_DX11.shaderbnd.dcx` (DCX zlib/BND3), exact six RX33/34/35 counterparts. Native DSR uses an additional fitted material Schlick chain `DP3 -> MAD(-5.55472994,-6.98316002) -> MUL -> EXP2 -> MAD`. The 5 instructions are byte-for-byte identical within each Stable/Lerp matched shader (shift +82 DXBC DWORD):

| RX | Stable positions | Lerp positions |
|---|---|---|
| RX33 | 2233,2240,2249,2256,2261 | 2315,2322,2331,2338,2343 |
| RX34 | 2152,2159,2168,2175,2180 | 2234,2241,2250,2257,2262 |
| RX35 | 1805,1812,1821,1828,1833 | 1887,1894,1903,1910,1915 |

**Important falsifier:** Schlick is not exclusive to Lerp. Its mere existence does not explain a film switched on only during Lerp. The *inputs reaching this common tail* may differ.

## V336 R19/R8 composition

`renderer-core/src/operators/env_spec/pmetal_rgba_lerp_materializer.cpp` builds PTDE RGB/A decode plus A/B mixed EnvSpec and isolated R15–17 material workflow; `pmetal_envspec_draw_runtime.cpp` places source A into b12[2], B into b12[3].rgb and beta into b12[3].w. Nonmetal R8 modifies **only** a branch-local LOG/MUL(2.2)/EXP to MOV (ELSE of DSR `IF cb0[85].x`) and dedicated t10 SpecRGB source swizzle `wxyz -> xyzw`. It does NOT certify reconstruction of all later DSR material/Schlick consumers. Separate stock t1 and sidecar t10 source paths remain. Runtime activation and effect on the actual output for those late ops **OPEN**.

## Next diagnostic (no cubemap changes)

Build exact final V336 patched Stable/Lerp shader pairs from the same signed SHA and feature flags; trace common EnvSpec merge, SpecRGB/c101/COLOR0, downstream native t1/F0/Schlick inputs, CB85 branch, and preFog output. Demand Stable↔Lerp endpoint continuity with identical draw/material/source and producer states. No global Schlick disable, blanket PBL change, arbitrary gain, or slot-specific reinterpretation without proven consumer cause. **No binary/addon modification in this audit.**

**Validation:** independent offline disassembly and 6/6 paired-bytecode verification, plus original V336 indexed-issue source evidence. Runtime pixel PASS remains OPEN.
