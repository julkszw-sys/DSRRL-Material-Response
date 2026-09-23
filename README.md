# DSRRL Material Response

Source/build material for the **exact Material Response 1.45 file uploaded to Nexus Mods** for Dark Souls Remastered – Restored Lighting.

The addon is loaded by ReShade and changes selected verified renderer/material paths inside Dark Souls Remastered. DSR remains the host renderer; unsupported or unmapped routes fail open to the stock game path.

## Exact Nexus release identity

Nexus archive:

`DSRRL_Material_Response_1.45.addon64(20260923-005017).zip`

- ZIP SHA-256: `2439d24644a0dc5bef6e29b1d980bab5b421a08c6bc71fca93d4c88859ad0893`
- ZIP size: `244,154 bytes`

Contained addon:

`DSRRL_Material_Response_1.45.addon64`

- SHA-256: `e44183ef10fc7921f7741eb16d54ec30e814f580421ac6c83be18b5308b73342`
- Size: `1,803,264 bytes`
- PE checksum: `0x001BC666`
- Version: `1.45.0.0`

This identity supersedes the earlier review-branch hash `3dcb50...`, which was the clean intermediate before the final P_Metal terminal-SAT delta.

## Release scope

This Nexus release intentionally ships **without active PTDE EnvSpec/cubemap replacement**.

Included in the binary:

- Material Response corrections
- PTDE SpecRGB transport
- subsurface material handling
- equipment diffuse and normal bridge code/path
- terminal RGB saturation on the final output write of embedded DXBC 33/34/35
- guarded fail-open behaviour for unsupported/unmapped routes

Disabled/removed for release:

- PTDE EnvSpec/cubemap substitution
- external PackedGI loader and PackedGI dependency
- one-shot bridge activation telemetry
- inherited periodic counter/status telemetry callback
- startup shader-count telemetry

Safety/error/fail-open logging is retained.

The SAT delta is deliberately described at the binary-consumer level: this branch does not claim a broader material scope than the existing runtime routing proves.

## Exact build path

See [BUILD.md](BUILD.md).

The final Nexus file is reproduced in two deterministic stages:

1. exact integrated 1.45 basis -> clean EnvSpec-disabled intermediate `3dcb50...`;
2. exact terminal RGB SAT patch -> Nexus shipping file `e44183...`.

The second stage is fully public in:

`tools/patch_pmetal_terminal_sat.py`

and is byte-for-byte verified against the Nexus upload.

## Release verification

Run:

```text
python tools/verify_release.py DSRRL_Material_Response_1.45.addon64
```

The verifier checks exact file identity, EnvSpec-disable invariants, the embedded DXBC hashes and the terminal `MOV_SAT` writes in DXBC 33/34/35.

## Source review

See:

- [docs/NEXUS_REVIEW.md](docs/NEXUS_REVIEW.md)
- [docs/NEXUS_SOURCE_REVIEW.md](docs/NEXUS_SOURCE_REVIEW.md)
- [docs/EXACT_NEXUS_RELEASE_DELTA.md](docs/EXACT_NEXUS_RELEASE_DELTA.md)
- [SECURITY.md](SECURITY.md)

The repository distinguishes inspectable runtime source from deterministic exact-binary materialization instead of claiming that an incrementally developed shipping binary is a byte-identical compiler output of the review C++ target.
