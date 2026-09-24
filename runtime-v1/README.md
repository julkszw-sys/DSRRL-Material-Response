# Renderer Core integrated runtime

Build from the repository root:

```sh
cmake -S runtime-v1 -B build/runtime-v1 -A x64 -DRESHade_INCLUDE_DIR=PATH/TO/reshade/include
cmake --build build/runtime-v1 --config Release --parallel
ctest --test-dir build/runtime-v1 -C Release --output-on-failure
```

Use ReShade headers at commit `3645e3025d1d98a90e318278858931f034d5d1f6` (API20).
The Windows CI performs the same build and audits exactly one canonicalized
`DSRRL_Renderer_Core_Runtime_v1.addon64`. No predecessor addon binary is used.

## Install for runtime validation

Install that one addon in the game directory with the addon-enabled ReShade
API20 host. Remove other DSRRL addon versions from the load directory first:
they own overlapping engine hook sites. Existing asset sidecars are read from
`DSRRL/Specular`, `DSRRL/Diffuse` and `DSRRL/Normals`.
Exact EXE and shader binder SHA checks run before hook installation.
Unknown shader/material/resource routes remain stock.

## Wired islands

10 boot-armed operators: Material Response, SpecRGB, Diffuse, Normal,
diffuse material domain, terminal RGB SAT, PntS attenuation,
no-specular EnvSpec deletion, fixed post-fog identity, and Subsurface body bypass.
Upper/Lower is the eleventh operator, enabled only after exact producer preflight.

Subsurface is limited to the three certified stable HemEnv body receivers
33/34/35 and the exact DSR `Ps_Body[DSBT]` raw material SHA. It uses the verified
PTDE `Ps_Body[DSB]` donor and an ordinary shader variant materialized when the
host creates that target shader. All three sidecars must be ready and actually
bound in the same transaction. Only the two certified body specular logical
identities and exact Diffuse/Normal tuples are accepted. If the ordinary target
has not been created, or any dependency is missing, DSR Subsurf remains active.
This is a draw-specific reuse of a create-time materialized shader, never a
blanket replacement of a shared shader. `subsurface_replay` counts issued draws;
it does not establish visual equivalence.

## Remaining islands

HemDir3, EnvDiffuse, legacy EnvSpec, full PointLight, local legacy specular and
FaceEye remain OFF: their readiness contracts do not yet have complete live
producer/resource/shader wiring. Bloom/HDR remain blocked; RGBA SAT is rejected;
native SFX paths are preserved. No feature is enabled solely to raise a count.

## Verification scope

Linux unit tests check construction, exact routing policy and dependency gates.
Windows CI checks the native MSVC/MASM addon and single-addon output.
Game liveness, per-island activation and PTDE pixel behavior require separate
evidence; they are not asserted by this build.
