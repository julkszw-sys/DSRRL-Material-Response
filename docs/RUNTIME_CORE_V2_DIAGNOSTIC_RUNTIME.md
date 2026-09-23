# Runtime Core V2 Diagnostic Runtime

This is a pixel-inert ReShade API 20 diagnostic add-on for DSRRL.

## Install

Place `DSRRL_Runtime_Core_V2_Diagnostic.addon64` next to the active DSRRL Material Response add-on in the Dark Souls Remastered game directory.

It does not replace Material Response and does not modify renderer state.

## Expected log markers

Startup:

`[DSRRL RUNTIME CORE V2] READY API20 PIXEL-INERT`

Periodic heartbeat:

`[DSRRL RUNTIME CORE V2] P=... LIVE ... SRV_UPD t0=... t2=... t10=... t12=... t14=... DRAW_RESOLVE ...`

Identity health:

`[DSRRL RUNTIME CORE V2] IDENTITY miss/ambiguous ... STALE ...`

## What this runtime proves

- ReShade lifecycle/resource/view/pipeline hooks are active.
- Runtime Core V2 tracks D3D11 command lists independently.
- Resource descriptor identity is captured at resource creation.
- PS SRV bindings at t0/t2/t10/t12/t14 are visible to the core.
- Draw-time bound SRVs can be resolved generation-safely.
- stale handle reuse is rejected.

## What it does not prove

This standalone runtime does not share C++ state with the existing legacy Material Response binary. Therefore it does not, by itself, prove that the legacy PTDE replacement sidecar was selected or bound. The existing Stage Telemetry V2 build remains the bridge-specific companion diagnostic for:

- CS/CN/CD capture flags
- DG/DS/DIF Diffuse path
- NG/NS/NRM Normal path
- ENV EnvSpec
- SUB Subsurf

The intended owner test is to run both diagnostic add-ons in the same session and compare the logs.
