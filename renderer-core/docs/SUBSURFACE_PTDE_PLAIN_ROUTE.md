# Ps_Body Subsurface -> PTDE plain surface route

## Current target

The current route-partition target for body material is:

- DSR: `Ps_Body[DSBT].mtd`, `ColDifSpcBmpSubsurf`
- PTDE: `Ps_Body[DSB].mtd`, plain `ColDifSpcBmp`
- PTDE material/spec response: `c101 = 1.0`

This supersedes the older compatibility-only intermediate that preserved DSR
Subsurface Scattering while injecting PTDE SpecRGB at t13. That older build
remains useful for exact receiver/material routing evidence, but preserving DSR
SSS is not the current PTDE-visible target for this material route.

## Exact DSR receiver scope

Stable HemEnv / no-PointLight only:

| DSR Subsurf receiver | Exact stock SHA-256 | Target ordinary receiver |
| --- | --- | ---: |
| `FRPG_Phn_DifSpcBmp______Csd_HemEnvSubsurf.fpo` | `0b8288d686c8f349ad87352946be51ffd007462f25357326bf47e736e690e511` | 33 |
| `FRPG_Phn_DifSpcBmp______Sdw_HemEnvSubsurf.fpo` | `885337e50f3d29f086fd18e1f7524f28712031d0264964aef5f37037df7d7bcb` | 34 |
| `FRPG_Phn_DifSpcBmp__________HemEnvSubsurf.fpo` | `3002cfb9aee6835412399c3be267ab94c5706d7d5030fc4cafc82bc54c55a860` | 35 |

HemEnvLerp, PointLight and other Subsurf families are not authorized by this
contract.

## Exact material identity

- DSR `Ps_Body[DSBT].mtd` SHA-256:
  `2706080f2b306245b90bf9d3fdfa38624b2ed0452a9b1aff830cf59de69b37d4`
- PTDE `Ps_Body[DSB].mtd` donor SHA-256:
  `af2f108831b783a43b0e02f047919719d14f38e68d6c5a97b80678d593ba1c95`
- certified body SpecRGB identities:
  `BD_F_body_s` and `BD_M_body_s`

## Activation boundary

The route gate does not implement the shader bypass itself. It authorizes the
candidate only when all of the following are independently ready:

- actual DSBT material identity;
- exact one-of-three Subsurf receiver identity and stable HemEnv/no-PointLight
  draw path;
- exact body texture identity;
- exact slot-for-slot PTDE DSB donor mapping;
- explicit verification of the current plain-surface target;
- corresponding ordinary receiver 33/34/35;
- PTDE SpecRGB, Diffuse, Normal and Material Response routes;
- a carrier that actually excludes the DSR-only Subsurf/SSS contribution.

Any missing coordinate preserves the host route. Construction of this gate does
not promote runtime activation or PTDE-visible pixel equivalence.
