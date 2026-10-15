# V278 owner phenotype: P_Metal GOOD, Leather BAD during HemEnvLerp

Source: owner visual report immediately after V278 test and owner ReShade log `ReShade(20261015-044207).log`, SHA256 `eb201176129ff8c8c22ee1d2b0ca65f28913bd8c1c4c01ad91a6f3608c822cf6`.

**User-designated visual control**: P_Metal renders correctly; Leather shows laminate **during Lerp**. Exactly which physical Leather FLVER/slot/pixel is visibly BAD is not yet annotated. Do not apply BAD labels automatically to every Leather row or to DullLeather/Edge.

V278 application: RX33/RX34/RX35 all `R5_APPLIED`, no fallback. 216 Lerp prepared and issued draws, all with planned PS+t10+t12 (RX33 168, RX34 48). R5 is not a sufficient visual fix in the reported scene.

## Material-controlled source witnesses, not pixel matches

Paired `P_Leather[DSB]` versus `P_Metal[DSB]` records with **identical full FLVER SHA, host t1 SRV, PTDE t10 SpecRGB SRV, Lerp RX, fog12w/bucket, and planned shader**: **36 pairs** across **two physical slot pairs**, each sampled in **18** Lerp atmosphere states. Neither selected resource address proves sampled texture texels.

1. FLVER `eb3b77ee419f9e2997b7d427987c002411fdf08edda12834be4a9f51b3c4eb7b`: Leather slot2; Metal slot3; `t1=0000000072786d68`, `t10=0000000072789228`.
2. FLVER `672e35606495f24e36c704f32fceda9b933563a64e03379375f47f9fe04a0a7d`: Leather slot8; Metal slot5; `t1=0000000072786f28`, `t10=0000000072788968`.

In both paired rows: same Lerp phase, same shader object within each RX, same recorded CPU CB10=`1,1,1`, cb85=0, and same source probe endpoints IDs A=71/B=85 at V229 early/mid beta. V229 `GLOSS CUT` at beta≈0.0333 and ≈0.56667 confirms distinct **intended PTDE material response and probe selection**:
- Metal: material EnvSpec slot2; `c101=2.5`, `ptde_specpower=8.5`, selected `envspec_cubeA=8edb35e8`, `envspec_cubeB=9e316f28`.
- Leather: material EnvSpec slot1; `c101=1.5`, `ptde_specpower=4`, selected `envspec_cubeA=8edb2568`, `envspec_cubeB=8edb2fe8`.
- Source EnvDiffuse cubemaps A/B are shared in V229, while EnvSpec cubemaps differ by material slot. These differences are expected carrier features and present in Stable as well; **neither static c101 nor resource identity alone is the Lerp-only defect**.

**Critical diagnostic detail:** V275 P_Metal rows print `ptde_c101=0,ptde_c102=0` because P_Metal is an out-of-census diagnostic control. They must NOT override the authenticated V229 `GLOSS CUT` source `c101=2.5, ptde_specpower=8.5`.

Source code `renderer-core/src/operators/env_spec/pmetal_rgba_lerp_materializer.cpp` contains the verified Lerp `t10` sampling → raw `c101` multiply → color multiply → EnvSpec multiply chain, plus guarded R19 MaterialWorkflow and EnvSpec/EnvDiffuse merges. **Source candidate, NOT a proven bad operator**: verify in the actual GPU first which material-local input and Lerp consumer causes undesired glossy film for BAD Leather while the same operator gives correct Metal. A global SpecRGB/roughness/gain reduction is rejected: it would alter a known visually GOOD control.

**Next decisive test**: obtain visual BAD Leather pixel and the exact matched physical FLVER/slot, with GOOD Metal control and matched beta/camera; GPU Pixel History and intermediate N/reflect, SpecRGB*raw-c101, EnvSpec A/B, preFog diffuse/specular, postmerge, postFog. If first discrepancy is EnvSpec resource/probe mapping, patch resource; if material coefficient branch/workflow, patch shader/CB only in exact family. Stock Fog/LS/PBL unaffected.

Status: USER PHENOTYPE CONFIRMED (GOOD/BAD family level); source pair routing CONFIRMED; exact BAD pixel/slot, PTDE GPU operator and root cause OPEN. R5 negative as sufficient visual fix.
