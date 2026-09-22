# Security notes

Material Response is a ReShade addon for Dark Souls Remastered. It runs inside the game process and works on renderer state and D3D11 resources.

## Expected behaviour

The addon may:

- receive ReShade renderer callbacks
- inspect/track D3D11 resources and views
- select verified material/resource routes
- temporarily replace shader-resource bindings for a draw
- restore the original bindings afterwards
- read DSRRL material/equipment sidecar assets from the game directory when required by the enabled bridge

## It does not intentionally

- connect to remote servers
- perform HTTP requests
- download or execute programs
- launch external processes
- install services or drivers
- create scheduled tasks or autorun persistence
- collect credentials or personal data
- inspect browsers, email or unrelated applications
- modify `DarkSoulsRemastered.exe` on disk

## EnvSpec/cubemap status

PTDE EnvSpec/cubemap replacement is disabled in this clean release. The previous external PackedGI loader is not invoked, its injected loader cave is cleared, and the EnvSpec substitution gate is forced to fail open before the resource-replacement path.

No PackedGI EnvSpec file is required by this release build.

## Telemetry and logging

Release telemetry is disabled at the code path, not merely hidden by label removal:

- one-shot SpecRGB/Subsurf/Normal/Diffuse activation telemetry is bypassed
- the periodic counter/status callback is not registered
- the periodic callback entrypoint is inert as a second guard
- startup shader-count telemetry is disabled
- associated release telemetry strings are removed

Safety/error/fail-open logging is intentionally retained so incompatible or rejected routes remain diagnosable without collecting periodic runtime statistics.

## Antivirus detections

In-process renderer hooks and resource substitution can overlap with generic malware heuristics. The release branch keeps the transformation auditable and provides an exact SHA-256 plus a verifier for the shipped binary.
