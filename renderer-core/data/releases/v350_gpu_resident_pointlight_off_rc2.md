# V350 GPU Resident PointLight OFF RC2 — source descendant

- Source parent: frozen V350 Scope Shell GPU Resident RC1 `d1ead3cfd6d04aa456edde1cb694c3cfed086687`.
- This new candidate preserves the exact original RC1 binary and source branch unchanged.
- Requested operator policy: **addon PointLight OFF; stock DSR PointLight ON**.
- Compile `DSRRL_POINTLIGHT_DRAWTIME_BYPASS=ON`; this is the actual PointLight hook/create-time/draw-time bypass. `DSRRL_PHYSICAL_CUT_POINTLIGHT_ALL=ON` alone is only a historical V285 diagnostic prerequisite and does not disable PointLight.
- The enabled-operator registry explicitly sets PointLight, local-specular PointLight, and PntS attenuation to OFF under this bypass macro.
- No PointLight fixed/clustered producer hook installation, pipeline candidate creation, shader route binding, or draw transaction expected.
- Preserve P_Metal, HemEnvLerp V350 RGB, exact Material Response, SpecRGB/Diffuse/Normals authorization, GPU preload, DXGI half-free budget and independent postprocess.
- Preserve unchanged DarkSoulsRemastered.exe and only one integrated `.addon64`.

## Verification boundaries
Source and Windows CI construction is required. Native CI cannot prove DSR runtime activation or pixel equivalence; those remain OPEN until independently attested. Do not promote this child over either frozen V350 RC1 or official release based on source-only results.

## Rollback
Use the original immutable `DSRRL_V350_SCOPE_SHELL_GPU_RESIDENT_RC1.addon64` if the descendant has issues. Never deploy two DSRRL addons at once.
