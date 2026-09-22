# P_Metal Native Cube Exception — Current V1

## Scope

Diagnostic/operator-local compatibility workaround for the current Material Response 1.45 Stage Telemetry V2 lineage.

Owner runtime and visual evidence show that PTDE cubemap activation causes a severe energy collapse specifically on P_Metal, while other equipment/base textures remain visibly active. Prior global P_Metal EnvSpec operator-island candidates either did not rescue the black result or produced a strong cyan/aquamarine response.

## Change

At the existing shared PREPARE cut, preserve the current ordinary receiver gate but add one exact actual-material exception first:

```text
route = TLS[+0x10]
if route == 345:          # unique exact P_Metal[DSB].mtd route
    fail open             # keep native DSR t12/t14 for this material only

local = TLS[+0x14]
if local >= 23:
    fail open

continue unchanged
```

No route-table reclassification is performed. Route345 remains flag1, so the equipment asset bridge semantics are not changed.

## Invariants

- non-P_Metal EnvSpec path unchanged
- no DXBC changes
- SpecRGB unchanged
- Diffuse/Normal binders unchanged
- c101/COLOR0 unchanged
- PointLight unchanged
- EnvDiffuse unchanged
- PARAM/postprocess unchanged

This is not PTDE pixel equivalence for P_Metal. It is a narrow fail-open fallback to prevent the P_Metal-only cubemap energy collapse while retaining PTDE cubemap work for all other materials.

Basis SHA256:
`e15747f4920bd5f40db9019e4e45110ad4655b4defab1ad3027b039fc022d98b`

Output SHA256:
`3cb07d4f547fd0947ea132acb37b4b8c2410f426e55e012a3d053809222c5533`
