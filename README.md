# DSRRL Material Response

Source and build files for the Material Response component of **Dark Souls Remastered – Restored Lighting**.

The addon is loaded through ReShade and changes selected renderer/material paths inside Dark Souls Remastered. DSR remains the host renderer; unsupported or unknown routes fall back to the stock game path.

## Development

The repository now has an explicit split between release reproduction and source-first renderer development:

- `main` — shipped release and exact release-reproduction material
- `develop` — canonical Runtime Core V2 / source-first integration line
- short-lived experimental branches — isolated operator hypotheses only

The historical 1.45 release builder remains intentionally frozen around its SHA-pinned development basis. New renderer work should converge on a complete source build rather than adding further opaque binary patch stages.

See [docs/DEVELOPMENT.md](docs/DEVELOPMENT.md) and [CONTRIBUTING.md](CONTRIBUTING.md).

## Current release

**Material Response 1.45**

`DSRRL_Material_Response_1.45.addon64`

SHA-256:

`41690c6212157eb772ae0c75c055d0bb7a842709f65f02c3689b87714151e1f7`

Size:

`1,803,264 bytes`

## What it contains

The current addon includes renderer-side support for:

- material-response corrections
- PTDE SpecRGB transport
- subsurface material handling
- equipment diffuse and normal resource bridges
- PTDE PackedGI EnvSpec cubemap substitution
- guarded fail-open behaviour when a resource or route cannot be verified

The EnvSpec cubemap data is stored outside the addon at:

`DSRRL\EnvSpec\PackedGI\PTDE_GI_ENVSPEC_PACK_RGBA.bin`

The addon checks the file size and SHA-256 before enabling that resource path.

## Runtime Core V2 development

The `dev/runtime-core-v2` branch adds a clean-room runtime state/proof layer intended to consolidate routing used by SpecRGB, Diffuse, Normal, EnvSpec and Subsurf.

It adds:

- per-command-list state instead of process-global draw state
- generation-safe resource/view and pipeline lifetime tracking
- early semantic resource-descriptor fingerprints at resource creation
- generation-safe logical-resource lookup with fail-open on missing or ambiguous identity
- explicit route contracts for shader / receiver / material / resource / logical-ID / format / state proof
- source-level staged routing telemetry from capture through final bind/restore
- fail-open when a required identity is missing
- per-operator `matched / activated / restored / fail-open` counters
- frame-boundary detection of incomplete draw-state restore

The core is intentionally pixel-inert until individual existing bridges are migrated onto it.

See [Runtime Core V2](docs/RUNTIME_CORE_V2.md) and the [reference architecture audit](docs/DXGI7_ARCHITECTURE_AUDIT.md).

## Building

See [BUILD.md](BUILD.md).

The final 1.45 packaging/build step is reproducible from the exact development basis and the exact PackedGI resource file. The expected output hash is checked by the build script.

## Source

The readable EnvSpec loader implementation is in:

`src/envspec_loader.c`

The shipped release uses the equivalent fixed Win64 loader bytes stored in:

`reference/loader_bytes.hex`

Those bytes can be materialized with:

`python tools/materialize_loader.py`

## Security

The addon runs in-process with the game and therefore uses APIs that can look unusual to heuristic antivirus engines, including renderer hooks, resource tracking and controlled memory-protection changes.

It does not intentionally provide networking, downloading, persistence, services, drivers or process-launching functionality.

See [SECURITY.md](SECURITY.md) and [docs/NEXUS_REVIEW.md](docs/NEXUS_REVIEW.md).
