# DSRRL Core+Islands 2.0.0-dev — integrated single-addon target

This directory is the canonical integration root for the eventual single DSRRL `.addon64` on the active `main` line. Its product version is **Core+Islands 2.0.0-dev**, sourced from `VERSION.json`. It is deliberately a **construction/development target**, not an RC or release.\n\nMaterial Response **1.45 is a frozen legacy monolith** and belongs to a separate version namespace; it must not be used as the version identity of this target.

## Current Core+Islands 2.0.0-dev scope

The integrated entrypoint now combines the exact A1 create-time bridge with the
source-complete material/resource runtime glue recovered in `runtime-v1`.
The glue is compiled once as `dsrrl_runtime_v1_operator_glue`; the predecessor
standalone addon and this integrated target consume the same implementation
rather than maintaining divergent copies.

Exactly eleven operators are boot-armed by
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
- fixed post-Fog identity;
- owner-proven P_Metal V10 black-safe source-gain correction.

Material Response, SpecRGB, Diffuse, Normal, Subsurface and P_Metal V10 are
draw/material/resource/operator glue. The other five are the previously
integrated exact create-time A1 islands. V10 is restricted to exact route345
P_Metal, receivers 33/34/35, requires the verified Diffuse/material path, and
fails open to ordinary Material Response when its V9A replacement is unavailable.
It retains native DSR t12/t14 EnvSpec resources and is not permission to activate
the separate V13/U-L source island.

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

## Deferred producer/readiness glue

Partial operators are not promoted merely because their predecessor code exists.
Integrated now wires two **pixel-inert preflights** while keeping the visible
feature gates OFF:

- Upper/Lower producer capture: exact producer/assignment snapshots are observed
  and joined through the shared selector owner, but no b13 bind occurs while the
  U/L feature remains OFF. The separate P_Metal V13/A-B producer hook is
  explicitly disabled in this mode.
- legacy EnvSpec resource preflight: native probe/SRV identity, exact sidecar
  admission and PTDE sampler/resource carrier readiness may be established, but
  no t12/t14/s12/s14 substitution occurs while EnvSpec remains OFF.

Failure of either preflight is local and fail-open; it does not disable the 11
already-wired visible islands.

## Deliberately not armed

The presence of producer/readiness glue is not authorization to activate an
operator. The integrated policy therefore keeps these paths OFF:

- Upper/Lower;
- HemDir3;
- EnvDiffuse;
- legacy EnvSpec / P_Metal EnvSpec source;
- full PointLight and local legacy specular;
- FaceEye legacy shadow;
- P_Metal V13/A-B black-safe source;
- Bloom/HDR;
- rejected RGBA SAT.

Native DSR SFX paths remain host-preserved.

## Status semantics

CI may establish source completeness, construction and ABI compatibility.
Runtime liveness, exact receiver hits, bridge activation and PTDE-visible pixel
behavior are separate statuses. A green build does not promote any of those.
