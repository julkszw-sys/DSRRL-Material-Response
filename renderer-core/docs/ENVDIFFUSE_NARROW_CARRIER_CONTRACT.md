# Renderer Core v1 — EnvDiffuse Narrow Carrier Contract

Status: **CONFIRMED carrier/sampler boundary; producer-class assignment gated**

## Scope

This contract belongs only to `surface.envdiffuse`. It does not authorize EnvSpec, Upper/Lower, PointLight, material-domain, postprocess, runtime activation, or pixel equivalence.

## Source -> producer -> transport -> consumer -> composition -> downstream

1. **Source / DSR producer** — on the supported DSR executable, the operator-clean EnvDiffuse source cut is the output of the single/blended profile packers (`0x140563B80` / `0x140563C30`) before the later draw multiplier. Bridge only EnvDiffuse A/B XYZ through the confirmed inverse `q^(1/2.2)`; preserve alpha/beta and independent EnvSpec lanes.
2. **PTDE assignment producer** — assignment is producer-class scoped, not a generic FrpgModel-offset property. EnemyIns-owned ChrModel has a confirmed packed-GI selector producer: `0xE95BE0` constructs `packed_id = 10000*subarea + localProbe`; `0xE920C0` writes the inherited FrpgModel selector at `+0x7A` and dirty bit `0x100`, which feeds the confirmed generic selector-transition/environment-resource consumer. REMO/RemoPartsIns has a separate scoped route. Ordinary gameplay MapModel assignment remains OPEN.
3. **Forbidden attribution** — inherited ChrModel `FrpgModel+0x6C/+0x70` are a 64-bit display mask on the audited path. The former `+0x6C/+0x70` / tag `0x803A` GI-producer join is canonical REJECTED and MUST NOT satisfy assignment readiness.
4. **Transport** — endpoint state remains a CPU/source-domain semantic carrier. Exact independently assigned PTDE probe A is transported on `t11/s11`; probe B on `t13/s13` for applicable HemEnvLerp. Producer-class/selector provenance must travel with the resource-side assignment certificate; resource identity alone is insufficient.
5. **Consumer** — only certified EnvDiffuse receiver bodies may consume the bridge. Representative PTDE Phn HemEnv RE samples the assigned probe with the final shaded normal and decodes `RGB/A`.
6. **Composition** — local EnvDiffuse is environment visibility × decoded probe × bridged profile endpoint before the common legacy diffuse-material product. Independent diffuse lighting, specular, Upper/Lower and PointLight remain separately owned islands.
7. **Downstream** — preserve the existing receiver continuation, atmosphere/fog/terminal output and postprocess unless independently certified by their own islands.

## Narrowest carrier

`pre-draw-multiplier EnvDiffuse endpoint XYZ + exact PTDE probe SRV/sampler sidecar + producer-class/selector assignment provenance`

The carrier MUST NOT use a generic FrpgModel field pair, DSR-bound t11/t13 identity, or resource-name coincidence as a substitute for independent PTDE assignment.

## Sampler closure

The PTDE ordinary environment-cube sampler target is closed for this island: anisotropic MIN/MAG, linear MIP, U/V MIRROR, W WRAP, MaxAnisotropy=1, with the bridge realization constrained to mip0-only resources. Sampler recovery is therefore no longer the assignment blocker.

## Fail-open gates

Preserve stock DSR if any of these are unresolved or ambiguous:

- producer class is unknown or outside a separately certified class;
- ordinary MapModel assignment is requested without a recovered MapModel producer/update law;
- REMO and ordinary gameplay assignment are conflated;
- same-world PTDE↔DSR assignment relation for the enabled class is not independently verified;
- exact PTDE companion probe resource identity/ownership is missing;
- receiver-family applicability / HemEnv vs HemEnvLerp route is unresolved;
- endpoint/profile identity or beta preservation is unresolved;
- resource lifetime or draw-local restore is unresolved.

## Closed vs OPEN

Closed: endpoint source cut, endpoint inverse, t11/t13 receiver slots, sampler target, EnemyIns/ChrModel PTDE selector producer topology, and rejection of the display-mask pseudo-producer.

OPEN: ordinary MapModel producer/update law; same-world PTDE↔DSR assignment homology for production scope; source-complete live resource ownership/bind/restore; runtime activation; PTDE-visible pixel equivalence.

Raw evidence for the 2026-09-25 boundary refinement: `b1ce1012-070e-4157-beb8-50cb6fcbdc41`.
