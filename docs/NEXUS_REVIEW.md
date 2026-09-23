# Nexus Mods review notes

Mod: **Dark Souls Remastered – Restored Lighting**  
Nexus mod ID: **1423**  
Component: **Material Response 1.45 clean release**

## File under review

`DSRRL_Material_Response_1.45.addon64`

SHA-256:

`3dcb50bee7d4a1ffcb47c2e9d116cbad5da322f6719e3cf2ecdbe6846db63000`

Size: `1,803,264 bytes`

Type: 64-bit ReShade addon / PE DLL.

## Purpose

The addon changes selected rendering behaviour inside Dark Souls Remastered. This release contains Material Response, PTDE SpecRGB transport, subsurface handling, and equipment Normal/Diffuse bridges.

PTDE EnvSpec/cubemap replacement is intentionally not included in the active release path.

It is not a launcher or installer.

## Release hardening

Compared with the integrated development basis, the public clean release:

- bypasses/removes SpecRGB/Subsurf/Normal/Diffuse activation telemetry
- disables the inherited startup shader-count telemetry log
- does not register the inherited periodic counter/status telemetry callback and makes its entrypoint inert as a second guard
- retains only safety/error/fail-open logging
- skips the external EnvSpec loader
- forces the EnvSpec replacement gate to fail open
- clears the now-unreferenced injected EnvSpec loader cave
- does not require a PackedGI EnvSpec sidecar

The transformation is reproducible with `tools/build_release.py` and verified by `tools/verify_release.py`.

## Why heuristic scanners may flag it

The addon runs in-process with the game and performs renderer hooks, D3D11 resource tracking, draw-scoped resource substitution/restoration and controlled renderer-state work. Those operations are normal for this mod but can overlap with generic malware heuristics.

## Build material

Relevant files in this repository:

- [BUILD.md](../BUILD.md)
- [SECURITY.md](../SECURITY.md)
- [tools/build_release.py](../tools/build_release.py)
- [tools/verify_release.py](../tools/verify_release.py)
- [RELEASE_HASHES.md](../RELEASE_HASHES.md)

The clean release is reproduced from the exact integrated 1.45 basis identified in `BUILD.md`.


## Public source review branch

Nexus staff can inspect and build the public source-review target here:

`nexus-review/material-response-1.45-source`

Full build/review instructions:

`docs/NEXUS_SOURCE_REVIEW.md`

The source-review branch also contains the exact clean-release materializer and verifier. The shipping binary was developed incrementally, so the public C++ review target is explicitly separated from the exact SHA-256 reproduction path; the exact integrated basis can be supplied directly to Nexus staff if requested.
