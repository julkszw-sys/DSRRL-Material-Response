# Renderer Core v1 — EnvDiffuse Narrow Carrier Contract

Status: **CONFIRMED carrier/sampler boundary; producer-class assignment gated**

## Scope

This contract belongs only to `surface.envdiffuse`. It does not authorize EnvSpec, Upper/Lower, PointLight, material-domain, postprocess, runtime activation, or pixel equivalence.

## Source -> producer -> transport -> consumer -> composition -> downstream

1. **Source / DSR producer** — on the supported DSR executable, the operator-clean EnvDiffuse source cut is the output of the single/blended profile packers (`0x140563B80` / `0x140563C30`) before the later draw multiplier. Bridge only EnvDiffuse A/B XYZ through the confirmed inverse `q^(1/2.2)`; preserve alpha/beta and independent EnvSpec lanes.
2. **PTDE assignment producer is class-partitioned** — ordinary gameplay MapModel is CONFIRMED Classic/legacy, not packed-GI: its constructor/seed path forces state1 low7=1, propagates packed-enable bit0x20 clear, produces draw `B0=-1`, and selects legacy resolver `0xF75380`. MapModel therefore does not consume the inherited `+0x7A` packed selector on this ordinary route. EnemyIns-owned ChrModel is a separate packed-GI class: `0xE95BE0` constructs `packed_id = 10000*subarea + localProbe`; `0xE920C0` writes FrpgModel `+0x7A` under dirty bit `0x100`. REMO/RemoPartsIns is another separately scoped packed class.
3. **Independent writer census** — exhaustive direct dirty-`0x100` selector-writer census finds only EnemyIns/ChrModel and RemoPartsIns families in the retained PTDE/DSR executables; no ordinary MapModel direct packed-selector writer exists. This supports the route partition but does not prove absence of every possible indirect/deserialization write outside the certified ordinary constructor path.
4. **Forbidden attribution** — inherited ChrModel `FrpgModel+0x6C/+0x70` are a 64-bit display mask on the audited path. The former `+0x6C/+0x70` / tag `0x803A` GI-producer join is canonical REJECTED and MUST NOT satisfy assignment readiness.
5. **Transport** — endpoint state remains a CPU/source-domain semantic carrier. Exact independently assigned PTDE probe A is transported on `t11/s11`; probe B on `t13/s13` for applicable HemEnvLerp. Producer-class/route provenance must travel with the resource-side assignment certificate; resource identity alone is insufficient.
6. **Consumer** — only certified EnvDiffuse receiver bodies may consume the bridge. Representative PTDE Phn HemEnv RE samples the assigned probe with the final shaded normal and decodes `RGB/A`.
7. **Composition** — local EnvDiffuse is environment visibility × decoded probe × bridged profile endpoint before the common legacy diffuse-material product. Independent diffuse lighting, specular, Upper/Lower and PointLight remain separately owned islands.
8. **Downstream** — preserve the existing receiver continuation, atmosphere/fog/terminal output and postprocess unless independently certified by their own islands.

## Narrowest carrier

`pre-draw-multiplier EnvDiffuse endpoint XYZ + exact PTDE probe SRV/sampler sidecar + producer-class/route assignment provenance`

For ordinary MapModel the provenance is `CLASSIC_LEGACY`; for certified packed classes it includes the exact packed selector provenance. The carrier MUST NOT use a generic FrpgModel field pair, DSR-bound t11/t13 identity, or resource-name coincidence as a substitute for independent PTDE assignment.

## Sampler closure

The PTDE ordinary environment-cube sampler target is closed for this island: anisotropic MIN/MAG, linear MIP, U/V MIRROR, W WRAP, MaxAnisotropy=1, with the bridge realization constrained to mip0-only resources. Sampler recovery is therefore no longer the assignment blocker.

## Fail-open gates

Preserve stock DSR if any of these are unresolved or ambiguous:

- producer class/route is unknown or outside a separately certified class;
- ordinary MapModel is incorrectly requested through the packed-GI route;
- REMO and ordinary gameplay assignment are conflated;
- same-world PTDE↔DSR assignment/resource relation for the enabled class/route is not independently verified;
- exact PTDE companion probe resource identity/ownership is missing;
- receiver-family applicability / HemEnv vs HemEnvLerp route is unresolved;
- endpoint/profile identity or beta preservation is unresolved;
- resource lifetime or draw-local restore is unresolved.

## Closed vs OPEN

Closed: endpoint source cut, endpoint inverse, t11/t13 receiver slots, sampler target, ordinary PTDE MapModel Classic/legacy route classification, EnemyIns/ChrModel packed selector producer topology, direct packed-selector writer class partition, and rejection of the display-mask pseudo-producer.

OPEN: same-world Classic PTDE↔DSR MapModel assignment/resource homology; same-world packed source-value homology for packed classes; source-complete live resource ownership/bind/restore; runtime activation; PTDE-visible pixel equivalence.

Raw evidence for the MapModel route refinement: `da6e779c-135d-4a93-87b3-b431f6930848`.
