# Renderer Core integrated single-addon construction target

This directory is the canonical integration root for the eventual single DSRRL
`.addon64`. It is deliberately a **construction/development target**, not an
RC or release.

## Current integrated A2 scope

The integrated entrypoint now combines the exact A1 create-time bridge with the
source-complete material/resource runtime glue recovered in `runtime-v1`.
The glue is compiled once as `dsrrl_runtime_v1_operator_glue`; the predecessor
standalone addon and this integrated target consume the same implementation
rather than maintaining divergent copies.

Exactly ten operators are boot-armed by
`dsrrl::runtime::k_integrated_feature_policy`:

- Material Response;
- SpecRGB resource bridge;
- Diffuse resource bridge;
- Normal resource bridge;
- Subsurface body bypass;
- terminal RGB SAT;
- diffuse material-domain;
- PntS attenuation;
- certified no-Spc EnvSpec deletion;
- fixed post-Fog identity.

The first five are draw/material/resource glue. The latter five are the
previously integrated exact create-time A1 islands.

## Material/resource transaction

The runtime uses one EngineBridge selector/MTD/texture-name owner. It requires
the exact supported EXE and shader-binder provenance before installing hooks.

For supported material draws the existing runtime transaction can select the
verified PTDE material-response shader and independently bind:

- PTDE SpecRGB at `t10`, while retaining stock DSR `t1` alpha/roughness;
- PTDE Diffuse at `t0` only together with its exact c100/material-domain route;
- PTDE Normal at `t2` only for an authorized homologous material route or
  certified exact `t0+t1+t2` tuple;
- the exact `Ps_Body[DSBT]` -> PTDE plain `Ps_Body[DSB]` Subsurface bypass
  only when the ordinary receiver and all dependent surface bridges are ready.

State restoration is part of the transaction. Unknown material, receiver,
logical texture, sidecar, tuple, donor, shader identity, device or restore
state fails open to the untouched stock DSR draw. A failed restore quarantines
the resource path rather than allowing a hybrid.

## Deliberately not armed

The presence of predecessor code is not authorization to activate an operator.
The integrated policy therefore keeps these paths OFF:

- Upper/Lower;
- HemDir3;
- EnvDiffuse;
- legacy EnvSpec / P_Metal EnvSpec source;
- full PointLight and local legacy specular;
- FaceEye legacy shadow;
- P_Metal black-safe source and V10;
- Bloom/HDR;
- rejected RGBA SAT.

Native DSR SFX paths remain host-preserved.

## Status semantics

CI may establish source completeness, construction and ABI compatibility.
Runtime liveness, exact receiver hits, bridge activation and PTDE-visible pixel
behavior are separate statuses. A green build does not promote any of those.
