# DSRRL Material Response Expanded A3 — monolithic dev branch

This branch is the persistent development line for the **single-addon** successor to
`DSRRL_Material_Response_1.45.addon64`.

## Goal

Produce one installable `.addon64` that preserves the shipping Material Response
1.45 behavior while integrating the renderer work that was previously split across:

- shipping Material Response 1.45,
- Expanded A1 global closed Material Response,
- Expanded A2 PntS/PntSS/PntSSSS receiver census,
- PTDE Upper/Lower runtime bridge.

The user should not need a stack of companion addons.

## Immutable basis

Shipping 1.45:

- SHA-256: `e44183ef10fc7921f7741eb16d54ec30e814f580421ac6c83be18b5308b73342`
- size: 1,803,264 bytes
- PE checksum: `0x001BC666`
- source/release branch: `nexus-review/material-response-1.45-source`
- source head used as branch basis: `b1a61bac857d50dd1a5c4a51c85d106583d38b24`

Any A3 build must verify this exact basis before integration.

## Integrated scope

### Shipping 1.45 behavior — preserve

- PTDE Material Response paths already present in 1.45
- SpecRGB bridge
- Subsurf bridge
- equipment Normal/Diffuse bridges
- black-armor/P_Metal shipping behavior
- EnvSpec external/resource replacement remains disabled/fail-open

### Expanded Material Response — integrate

A1 construction basis:

- 144 unique CONFIRMED+CLOSED shader bodies
- 252 aliases
- deterministic persistent per-plan materialization
- global renderer scope, not equipment-only
- shared `FRPG_Phn_DifSpc*` hosts remain material-gated/fail-open when exact material identity is absent

A2 diagnostic basis:

- 249 PntS/PntSS/PntSSSS receiver bodies
- 609 aliases
- read-only first-bind census
- ambiguous PntSS/PntSSSS shared bodies represented explicitly rather than guessed
- no visible patch from telemetry alone

### PTDE Upper/Lower — integrate

Verified runtime carrier:

- PTDE Upper -> PS `b13[6].xyz`
- PTDE Lower -> PS `b13[7].xyz`
- PTDE-linear domain
- draw-local transaction
- full state restore
- fail-open when routing/receiver/material identity is not certified

Payload generation currently covers:

- 48 stock DSR HemEnv/HemEnvLerp hosts
- 3 special P_Metal shipping-1.45 alternates for shader indices 894/913/932

The P_Metal variants are generated **from the shipping 1.45 embedded alternates** and
then receive only the U/L consumer delta. This prevents U/L testing from rolling
P_Metal back to an older P2.2/vanilla material response.

## EnvDiffuse policy

EnvDiffuse runtime resource replacement is **OFF in A3-U/L**.

This branch intentionally isolates Upper/Lower first. A later commit may add a
separately audited EnvDiffuse amplitude + probe/resource bridge, but it must satisfy
the existing EnvDiffuse PROTECT and exact assignment/homology requirements.

EnvSpec stays under the existing project policy:

- delete/disable only where the PTDE homolog is proven to have no EnvSpec;
- no blanket PTDE cubemap replacement in this A3 stage.

## Safety / routing invariants

- Shader identity is not material identity.
- Shared P_Metal-capable bodies are not globally promoted to P_Metal.
- Exact material routing is required where the existing P_Metal PROTECT applies.
- Unknown route / receiver / material / ABI -> fail open to existing 1.45 behavior.
- No arbitrary gain compensation.
- No PointLight tuning is part of the U/L integration.
- Build/runtime success does not imply pixel equivalence.

## Development layout

`experimental/expanded-a3/`

- `README.md` — this contract
- `src/` — A1/A2/A3 integration source
- `tools/` — deterministic generators/build integration tooling
- `generated/` — generated manifests/tables when small enough to review
- `audit/` — hashes, payload provenance and build audits

Large generated DXBC byte arrays should be reproducible from the audited generator
and exact input hashes rather than hand-edited.

## Current status

Construction of the monolithic A3 successor is in progress.

Do not tag or publish this branch as a release until:

1. exact 1.45 preservation is audited,
2. A1/A2 integration is construction-verified,
3. U/L draw-local activation is runtime-proven,
4. full state restore is runtime-proven,
5. pixel behavior is tested against PTDE,
6. Supabase control-plane and knowledge hygiene checks pass.
