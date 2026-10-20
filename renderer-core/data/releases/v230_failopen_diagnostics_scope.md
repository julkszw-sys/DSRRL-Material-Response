# v2.3.0 fail-open diagnostic variant

Base: EnvSpec Ceaseless Upper donor 170 + stock DSR EnvDiffuse, source commit `7646609416055c38307b33c3815f3cebb15292aa`.

## What is measured

The P_Metal EnvSpec draw preparation frontier records first observed rejection per reason, family, receiver and material route (candidate, operator policy, semantic identity, LightBank source, blend, D3D context, replacement shader, probe/SRV, SpecRGB material request, shadow and draw transaction preparation). Otherwise silent device and CB create gate are also classified. The existing full source decode frontier is retained at WARNING severity for source rejection, even after v2.3.0 production INFO suppression.

`[DSRRL FAILOPEN V230]` events identify actual reached runtime gates. `kind=REJECT` means the addon preserved stock DSR for that unsupported draw or operator. Such a reject is **not necessarily a defect**: unrelated stock materials are supposed to fail open. Count only the relevant `route=345`/certified RX33..35 and their map/material context as evidence of an unexpected closure.

## Instrumentation limitations

This first candidate instruments P_Metal EnvSpec prepare and the source decode frontier; it is **not** a verified exhaustive census of all Material Response, PointLight or resource-request nonmetal fail-open reasons. Telemetry activation and pixel improvement require game logs and A/B captures. No shaders, resources, profile donor values, render callbacks, or game executable are modified by this instrumentation. The CI runner confirms construction only.

## Log collection

Use the ReShade log file next to the game's `dxgi.dll` / ReShade loader after loading the map and moving through the target scene. Filter lines containing `DSRRL FAILOPEN V230`, `DSRRL ENVSPEC SOURCE`, or `SOURCE FAIL-OPEN`. Distinguish no receiver hit, identified receiver rejection, source/probe absence, and later shader/transaction failures. Compare with last stock-EnvDiffuse/EnvSpec170 build.
