# Nexus Mods review notes

Mod: **Dark Souls Remastered – Restored Lighting**  
Nexus mod ID: **1423**  
Component: **Material Response 1.45 clean release**

## File under review

`DSRRL_Material_Response_1.45.addon64`

SHA-256:

`13e722f9472e00c1922baecefe121bf1b7b9dd6129f1d2028d64568d74bb5ab7`

Size: `1,803,264 bytes`

Type: 64-bit ReShade addon / PE DLL.

## Purpose

The addon changes selected rendering behaviour inside Dark Souls Remastered. This release contains Material Response, PTDE SpecRGB transport, subsurface handling, and equipment Normal/Diffuse bridges.

PTDE EnvSpec/cubemap replacement is intentionally not included in the active release path.

It is not a launcher or installer.

## Release hardening

Compared with the integrated development basis, the public clean release:

- removes the SpecRGB/Subsurf/Normal/Diffuse/EnvSpec activation telemetry paths and strings
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
