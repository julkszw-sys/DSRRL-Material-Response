# DSRRL Material Response

Source/build material for the **Material Response 1.45 clean release** of Dark Souls Remastered – Restored Lighting.

The addon is loaded by ReShade and changes only selected verified renderer/material paths inside Dark Souls Remastered. DSR remains the host renderer; unsupported or unmapped routes fail open to the stock game path.

## Release branch scope

This branch intentionally ships **without PTDE EnvSpec/cubemap replacement**.

Included runtime features:

- material-response corrections
- PTDE SpecRGB transport
- subsurface material handling
- equipment diffuse and normal resource bridges
- guarded fail-open behaviour for unsupported/unmapped routes

Removed/disabled for release:

- EnvSpec/cubemap substitution
- external PackedGI loader and PackedGI dependency
- development/activation telemetry for SpecRGB, Subsurf, Normal, Diffuse and EnvSpec

The exact release addon remains a ReShade addon (`.addon64`). ReShade itself remains a separate `dxgi.dll`; this branch does not fork or embed ReShade.

## Current clean binary

`DSRRL_Material_Response_1.45.addon64`

Size: `1,803,264 bytes`

SHA-256: `13e722f9472e00c1922baecefe121bf1b7b9dd6129f1d2028d64568d74bb5ab7`

Version: `1.45.0.0`

## Building

See [BUILD.md](BUILD.md).

The clean release is reproduced from the exact integrated 1.45 basis listed there. No EnvSpec/PackedGI sidecar is required for this build step.

## Release verification

Run:

```text
python tools/verify_release.py DSRRL_Material_Response_1.45.addon64
```

The verifier checks exact identity plus release invariants: telemetry absent, EnvSpec loader removed, and the EnvSpec resource path forced fail-open.

## Security

The addon runs in-process with the game and uses renderer hooks/resource tracking. It does not intentionally provide networking, downloading, persistence, services, drivers or process-launching functionality.

See [SECURITY.md](SECURITY.md) and [docs/NEXUS_REVIEW.md](docs/NEXUS_REVIEW.md).
