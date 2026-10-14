# V284 — HemEnvLerp first: R5 normal + R11 common SAO continuity

**Status:** original stock RX preimage CONFIRMED; 3/3 local stock oracle PASS; final R19 owner-derived output, GPU/WARP, live bind, beta endpoints and PTDE pixel OPEN. **Do not ship or activate** based on source-only construction.

## Narrow operator conclusion

SHA-pinned original DSR Lerp RX33/34/35 have one and only one `t8/s8` SAO sample immediately followed by `mul r1.xyz,r1.xyz,SAO`. Stock word pairs:

| RX | t8 SAMPLE_L | Common surface MUL |
|---|---:|---:|
| 33 | 1878 | 1891 |
| 34 | 1787 | 1800 |
| 35 | 1440 | 1453 |

Stable R17 R11 already neutralizes the analogous DSR-only whole-surface SAO scalar after common EnvDiffuse/EnvSpec composition. The Lerp R19 production materializer currently has no R11 equivalent. Conversely, **the late `SpecTex.a` postmerge corresponding to Stable R3 is already neutralized** directly by `apply_exact_ptde_lerp_materialworkflow_r15_r16_r17`; do not patch it twice.

## Executable owner-local candidate

`reconstruct_lerp_r5_r11_from_final.cpp` derives every candidate from six original **FINAL** owner files: Stable R17 RX33-35 and Lerp R19 RX33-35. It first reconstructs exactly the known V281 R5 output and verifies each **whole-DXBC V281 SHA**. Only then does R11 replace **exactly the two source operand words** of the t8 SAO whole-surface multiply, using `1.0`. The sample, RDEF, signatures, stock Fog/LightScattering, RT1 Glow, postprocess and other instructions remain untouched. DXBC checksum is recalculated and validated. Any other layout, SHA, ambiguity or existing mutation fails open and emits no output.

Example for owner-local offline execution:

```sh
c++ -std=c++17 -O2 -Irenderer-core/include renderer-core/tools/v284/reconstruct_lerp_r5_r11_from_final.cpp -o v284
./v284 STABLE_R17_33.dxbc STABLE_R17_34.dxbc STABLE_R17_35.dxbc LERP_R19_33.dxbc LERP_R19_34.dxbc LERP_R19_35.dxbc EMPTY_OUTPUT_DIR
```

**No copyrighted original shader blobs are committed.** Source-only CI can compile this with invalid input and demand failure. Three original stock inputs were independently checked with a second Python reference: 3/3 exact t8 sites, 3/3 isolated two-word mutations, 3/3 legacy DXBC checksum fixes, and 15/15 negative corruption/identity/slot cases rejected. These tests **do not establish actual FINAL-R19 operator activation**. The independently reproduced stock oracle package SHA is `85d3a064f1b0ad8fdbe114df6c3dd2c4807523afef702710c88d2462d2e0a5fd`.

## Gating before live addon

1. Exact equipment FLVER SHA + original MTD raw SHA + material slot + current PS + physical PTDE SpecRGB sidecar identity; shared RX family alone is insufficient.
2. Verify final R5→R11 shader WARP CreatePixelShader and effective pre-Fog scene output. Stock DSR Fog/LS/Glow and independent PBL/PointLight are untouched.
3. Match PTDE A/B probe, `c84.w=c85.w=t`, DSR source beta at identical scene/time, and verify `beta=0`, `beta=1` endpoint continuity with Stable receivers; intermediate beta monotonicity is not automatically proof of correctness.
4. ReShade one-addon material-selective draw binding with exact full D3D state save/restore and fail-open. Runtime loaded/receiver/bridge/pixel statuses must stay separate.
5. Direct PTDE-visible matched-pixel comparison after activation before release.

V283 Stable gamma investigation remains independent and may not be used as a global Lerp gamma/exposure correction. R7 Csd/Sdw PTDE shadow kernel transfer remains OPEN; priority after R11 final-blob verification.

Source: PR #364, V284 source commit `f2972c7200d5e1880e16f5b9fd936c8808ba7722`; Supabase rev 12997.
