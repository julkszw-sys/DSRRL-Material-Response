# A1 Create-Time Diagnostic Addon

This target is a **diagnostic integration harness**, not the final DSRRL addon
and not a release candidate.

It connects the source-complete A1 create-time materializer to ReShade API 20:

`create_pipeline -> exact source SHA-256 -> per-island materialization ->
DXBC checksum -> init_pipeline SHA attestation -> bind telemetry`.

The diagnostic enables exactly five already-closed A1 shader islands:

- terminal RGB SAT;
- diffuse material-domain;
- PntS attenuation;
- certified no-Spc EnvSpec deletion;
- fixed post-Fog identity.

No draw callback, draw replay, EXE hook, P_Metal special case, resource
replacement, U/L carrier, or postprocess bridge is present.

## Fail-open behavior

Unknown source SHA, noncandidate code size, invalid DXBC, token mismatch,
materializer failure or unsupported device leave the original ReShade pipeline
description unchanged. A create->init replacement SHA mismatch quarantines
future materialization and is logged.

Replacement storage is persistent for the lifetime of the addon. Cache identity
is `(exact plan index, selected owner mask)`, so later integration can retain
independent island gates without conflating different replacement subsets.

## Telemetry

The log distinguishes create events, candidate-size hits, exact identity hits,
materializations, pass-throughs, fail-open, init attestations/mismatches and
target pipeline binds. First bind is logged once per exact plan.

These counters can establish runtime liveness and bridge activation after an
owner test. They do not establish PTDE-visible pixel equivalence.

## Build

The dedicated GitHub Actions workflow pins the same ReShade source commit used
by the source-reviewed Material Response 1.45 build:

`3645e3025d1d98a90e318278858931f034d5d1f6`

The workflow builds this directory on Windows x64, writes a build audit and
uploads a diagnostic ZIP. The shipping Phase-0 addon and Material Response 1.45
are not modified.
