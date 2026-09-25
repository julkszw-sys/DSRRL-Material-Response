# DSRRL Core+Islands 2.0.0-dev — integrated single-addon target

This directory is the canonical integration root for the active `main`
Renderer Edition. Product/version identity comes from `VERSION.json`.

**Material Response 1.45 is frozen legacy.** It is a separate historical
monolith and is not an active runtime dependency, library, addon base or
development surface for Core+Islands.

## Current active integrated scope

The current single addon links only native `renderer-core` code. Its visible
integrated set is the five source-complete A1 create-time islands:

- terminal RGB SAT;
- diffuse material-domain;
- PntS attenuation;
- certified no-Spc EnvSpec deletion;
- fixed post-Fog identity.

The entrypoint is `integrated_addon.cpp`. Shared create/init/bind attestation,
replacement cache, fail-open and quarantine behavior live in
`dsrrl::runtime::a1_create_pipeline_bridge`.

The broader Material Response, SpecRGB, Diffuse, Normal, Subsurface, P_Metal,
Upper/Lower, EnvSpec, HemDir3, PointLight and other islands remain represented
by native Renderer Core operator contracts/readiness code where available, but
are **not considered integrated merely because legacy 1.45/runtime-v1 once
implemented a related path**. They must be connected operator-by-operator using
native Core glue and current canonical routing.

## Legacy boundary

`runtime-v1/` and Material Response 1.45 may be consulted as frozen
provenance, historical evidence, regression reference or donor evidence only.
The active Core+Islands CMake target must not add that directory as a
subproject, link its libraries, compile its sources, install its hooks or use
its generated runtime as the execution layer.

CI explicitly audits this boundary.

## Safety boundary

The current integrated path is create-time only. It registers no draw callback
and performs no native draw replay. Unknown SHA-256, invalid DXBC, token
mismatch, unsupported device or attestation mismatch fail open. An init
mismatch quarantines further A1 materialization for that process.

## Status

Construction/ABI compatibility may be proven by CI. Runtime liveness, exact
receiver hits, bridge activation and PTDE-visible pixel behavior remain
separate statuses and are not inferred from a successful build.
