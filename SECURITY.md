# Security notes

Material Response is a ReShade addon for Dark Souls Remastered. It runs inside the game process and works on renderer state, shader payloads and D3D11 resources.

## Expected behaviour

The addon may:

- receive ReShade renderer callbacks
- inspect/track D3D11 resources and views
- select verified material/resource routes
- temporarily replace shader-resource bindings for a draw
- restore original bindings afterwards
- read DSRRL material/equipment sidecar assets from the game directory when required by an enabled bridge
- use embedded shader payloads selected by the addon renderer path

The exact Nexus 1.45 shipping addon is:

`e44183ef10fc7921f7741eb16d54ec30e814f580421ac6c83be18b5308b73342`

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

PTDE EnvSpec/cubemap replacement is disabled in this release. The external PackedGI loader is not invoked, its loader cave is cleared, and the EnvSpec substitution gate is forced to fail open before the resource-replacement path.

No PackedGI EnvSpec file is required by this release build.

## P_Metal terminal RGB SAT

The final Nexus build adds one narrow shader delta over the clean intermediate: the terminal RGB output instruction in embedded DXBC 33/34/35 has the DXBC saturate modifier set (`MOV -> MOV_SAT`). Alpha is untouched by this delta.

The exact public implementation is `tools/patch_pmetal_terminal_sat.py`. The verifier checks the final embedded shader hashes and SAT tokens.

## Telemetry and logging

Release telemetry is disabled at the code path, not merely hidden by label removal:

- one-shot SpecRGB/Subsurf/Normal/Diffuse activation telemetry is bypassed
- the periodic counter/status callback is not registered
- the periodic callback entrypoint is inert
- startup shader-count telemetry is disabled
- associated release telemetry strings are removed

Safety/error/fail-open logging is intentionally retained so incompatible or rejected routes remain diagnosable without collecting periodic runtime statistics.

## Antivirus detections

In-process renderer hooks, shader/resource state work and draw-scoped substitution can overlap with generic malware heuristics. The review branch exposes the security-relevant runtime source, the exact shipping materializer, the exact SHA-256 and a static verifier.
