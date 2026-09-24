# Renderer Core integrated single-addon construction target

This directory is the integration root for the eventual single DSRRL
`.addon64`. It is deliberately a **construction/development target**, not an
RC or release.

The first integrated runtime component is the exact A1 create-time shader
bridge. The standalone A1 diagnostic remains useful provenance, but the
shipping architecture must not accumulate one addon per operator.

## Current integrated islands

The shared Renderer Core `feature_registry` enables exactly these five
source-complete create-time islands:

- terminal RGB SAT;
- diffuse material-domain;
- PntS attenuation;
- certified no-Spc EnvSpec deletion;
- fixed post-Fog identity.

The event entrypoint lives only in `integrated_addon.cpp`. The reusable state,
replacement cache and create/init/bind attestation logic live in
`dsrrl::runtime::a1_create_pipeline_bridge`.

Future resource/material bridges should join this addon entrypoint rather than
introducing another independently loaded addon.

## Safety boundary

The integrated A1 corpus contains zero `FRPG_Phn_*Spc*` plans. Therefore this
create-time path does not enter the protected shared Phn DifSpc/DifSpcBmp
HemEnv P_Metal host class.

Unknown SHA-256, invalid DXBC, token mismatch, unsupported device or
attestation mismatch fail open. An init mismatch quarantines further A1
materialization for that process.

No draw callback or draw replay is registered.

## Status

Construction and ABI compatibility may be proven by CI. Runtime liveness,
actual receiver hits, bridge activation and PTDE-visible pixel behavior remain
separate statuses and are not inferred from a successful build.
