# Nexus Mods review notes

Mod: **Dark Souls Remastered – Restored Lighting**  
Nexus mod ID: **1423**  
Component: **Material Response 1.45**

## Exact file under review

Nexus archive:

`DSRRL_Material_Response_1.45.addon64(20260923-005017).zip`

- ZIP SHA-256: `2439d24644a0dc5bef6e29b1d980bab5b421a08c6bc71fca93d4c88859ad0893`
- ZIP size: `244,154 bytes`

Contained file:

`DSRRL_Material_Response_1.45.addon64`

- SHA-256: `e44183ef10fc7921f7741eb16d54ec30e814f580421ac6c83be18b5308b73342`
- Size: `1,803,264 bytes`
- PE checksum: `0x001BC666`
- Type: 64-bit ReShade addon / PE DLL

The previously documented `3dcb50...` addon was a clean intermediate and was not the final file uploaded to Nexus.

## Purpose

The addon changes selected rendering behaviour inside Dark Souls Remastered. The exact Nexus binary contains the Material Response renderer work, PTDE SpecRGB transport, subsurface handling, equipment Normal/Diffuse bridge code/path and the final terminal RGB SAT delta present in embedded DXBC 33/34/35.

PTDE EnvSpec/cubemap replacement is intentionally disabled in the release path.

It is not a launcher or installer.

## Exact final binary delta

The final Nexus file differs from the clean `3dcb50...` intermediate by only:

- terminal RGB output modifier `MOV -> MOV_SAT` in embedded DXBC 33/34/35
- the three affected DXBC checksums
- the PE checksum

The exact public implementation is:

`tools/patch_pmetal_terminal_sat.py`

Applying that script to the exact clean intermediate reproduces the uploaded Nexus addon byte-for-byte:

`e44183ef10fc7921f7741eb16d54ec30e814f580421ac6c83be18b5308b73342`

## Release hardening

The clean stage before the final SAT delta:

- bypasses/removes SpecRGB/Subsurf/Normal/Diffuse activation telemetry
- disables startup shader-count telemetry
- does not register the periodic counter/status callback and makes its entrypoint inert
- retains safety/error/fail-open logging
- skips the external EnvSpec loader
- forces the EnvSpec replacement gate to fail open
- clears the injected EnvSpec loader cave
- does not require a PackedGI EnvSpec sidecar

The transformation is reproduced by `tools/build_release.py` and checked by `tools/verify_release.py`.

## Why heuristic scanners may flag it

The addon runs in-process with the game and performs renderer hooks, D3D11 resource tracking, draw-scoped resource/state work and controlled renderer patching. Those operations are normal for this mod but overlap with patterns used by generic heuristic scanners.

## Review material

- [BUILD.md](../BUILD.md)
- [SECURITY.md](../SECURITY.md)
- [docs/NEXUS_SOURCE_REVIEW.md](NEXUS_SOURCE_REVIEW.md)
- [docs/EXACT_NEXUS_RELEASE_DELTA.md](EXACT_NEXUS_RELEASE_DELTA.md)
- [tools/build_release.py](../tools/build_release.py)
- [tools/patch_pmetal_terminal_sat.py](../tools/patch_pmetal_terminal_sat.py)
- [tools/verify_release.py](../tools/verify_release.py)
- [RELEASE_HASHES.md](../RELEASE_HASHES.md)

The branch deliberately distinguishes readable review source from exact incremental-binary reproduction. It does not claim that the shipping addon is a byte-identical compiler output of the C++ review target.
