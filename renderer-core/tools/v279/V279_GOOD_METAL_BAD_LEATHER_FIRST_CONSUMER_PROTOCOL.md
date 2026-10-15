# V279: exact GOOD Metal / BAD Leather Lerp transport witness

Owner phenotype: **P_Metal is visually correct**, **Leather laminates only during Lerp**. Owner V278 ReShade log SHA256 `eb201176129ff8c8c22ee1d2b0ca65f28913bd8c1c4c01ad91a6f3608c822cf6`. Full GLVER and MTD tuple mapping below comes from source, not from pixel picking.

## Two material-pair identities, source-corroborated
- full FLVER `eb3b77ee419f9e2997b7d427987c002411fdf08edda12834be4a9f51b3c4eb7b`: Leather slot2, Metal slot3, common `t1=72786d68`, `t10=72789228`.
- full FLVER `672e35606495f24e36c704f32fceda9b933563a64e03379375f47f9fe04a0a7d`: Leather slot8, Metal slot5, common `t1=72786f28`, `t10=72788968`.

Both share RX in the paired draw, Lerp shader interface object, same host SpecRGB/SpecTex view and same V229 prepared `b12`, source beta/probe IDs, and EnvDiffuse cubemaps. Metal is EnvSpec material slot2, c101=2.5, specpower=8.5; Leather EnvSpec slot1 c101=1.5 and specpower4; EnvSpec probe resource A/B views are different. These material-source distinctions also exist in Stable, so they are **not themselves transition-only errors**. V229 only logged sparse source samples; paired V275/V276 draw levels have more atmospheric buckets.

## First concrete shader operator frontier

Owner-private original V277 exact RX33, RX34 and RX35 Lerp STOCK+BRIDGE bytecode has exactly one D3D11 instruction `IF b0[85].x`. The byte-exact bridge changes **4 instructions** in its true-body (26 instruction block) and **2 instructions** in false ELSE-body (14 instruction block), in all 3 receiver families.

Within the ELSE-body, source changes are exactly:
1. constant-buffer factor `b0[9]` replaced by `b12[1]`;
2. direct float vector multiplier `2.2,2.2,2.2` replaced by `1,1,1`.

These are objective **shader equation differences** and potential sources of material-local susceptibility. CPU CB shadow prints b0[85]=0 but GPU execution branch is not physically verified; do not infer that the ELSE path executed in GPU. Source-only R5 restoration made no visible laminate change in owner V278, with create-time R5 applied and issuer on RX33/RX34; stop treating R5 as sole cause.

## Next diagnostic — V279 one read-only addon

Branch source from native V277 diagnostic ancestry, not from experimental R5 V278. 4 exact MTD+FLVER+slot material owners, RX33/RX34 only, Stable and Lerp. `v279_material_carrier_trace.hpp` observes the **complete prepared transaction** including operator owner masks, exact PS pointer, every b0-15 replacement CB with size/owners, t10/t12/t13/t14 SRV with format/view dimension, s10/s12/s14 descriptor including filter and LOD, plus *pretransaction* D3D11 b12 and t12/t14 pointers, CPU shadow and source atmospheric bucket. One log per paired source state, with fixed-size lock-free dedup. It does **NOT** read actual GPU CB values, reproduce a draw, change any shaders/CB/SRV/EXE, or claim PTDE pixel match.

After owner runtime, `analyze_v279_owner_carrier.py` forms exact phase/RX/asset/bucket Metal GOOD and Leather BAD pairs. Only promote a missing/incompatible material carrier where exact semantic mapping supports it. If carrier matches except intended c101/probe/slot, next step physical GPU first-consumer (EnvSpec/filter/postmerge/prefog), **not global specular gain**.

Status SOURCE CONTROL-FLOW VERIFIED / NEW NATIVE BUILD TBD / OWNER RUNTIME OPEN / GPU first divergent PTDE operator OPEN / PIXEL OPEN.
