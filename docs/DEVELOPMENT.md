# Development workflow

This repository has two different responsibilities and keeps them deliberately separate.

## Branch roles

### `main`

Release and release-reproduction line.

The current public Material Response 1.45 release is preserved here exactly. Its historical builder transforms a SHA-pinned development-basis binary and exact PackedGI resource into the published addon and verifies the final SHA-256.

Do not use `main` as an experiment branch.

### `develop`

Canonical source-first renderer development line.

Runtime Core V2, ReShade API integration, diagnostics and new operator bridges are integrated here before any release promotion.

New source architecture should converge here instead of creating another long-lived parallel branch.

### Experimental branches

Use short-lived branches only for isolated hypotheses or operator experiments.

Recommended naming:

`exp/<operator>/<purpose>`

An experimental branch must either:

1. be merged into `develop` after its construction/compatibility evidence is adequate; or
2. be left frozen as historical evidence and superseded by a newer branch.

Do not treat an experimental branch as a release lineage.

## Status gates

Every renderer change is evaluated independently at five levels:

1. CONSTRUCTION
2. COMPATIBILITY
3. RUNTIME LIVENESS
4. BRIDGE ACTIVATION
5. PIXEL-BEHAVIOR

A PASS at one level does not imply a PASS at the next.

In particular:

- compilation does not prove runtime activation;
- receiver/resource equality does not prove pixel equivalence;
- runtime activation does not prove PTDE-visible equivalence.

## Build layout

The pure Runtime Core V2 is always built.

ReShade-dependent targets require:

`RESHADE_INCLUDE_DIR`

Optional source targets are separated by intent:

- `DSRRL_BUILD_DIAGNOSTICS=ON`
- `DSRRL_BUILD_EXPERIMENTS=ON`

Example:

```text
cmake -S . -B build -DRESHADE_INCLUDE_DIR="PATH/TO/reshade/include" -DDSRRL_BUILD_DIAGNOSTICS=ON -DDSRRL_BUILD_EXPERIMENTS=ON
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

CI validates the pure core in both Debug and Release and compiles ReShade API 20 integration on Windows against the pinned upstream ReShade commit.

## Runtime Core invariants

Renderer bridges must preserve these invariants:

- command-list state is scoped per ReShade command list / D3D11 context;
- unknown command-list lifecycle does not silently create state;
- raw resource/view/pipeline handles are generation-checked;
- ambiguous logical resource identity fails open;
- route contracts must prove every required semantic before mutation;
- activation and restoration are counted separately;
- a missing restore is an integrity fault, not fail-open;
- unsupported or unknown routes remain stock DSR.

## Operator integration order

Move existing features onto the common runtime layer one operator at a time:

1. SpecRGB
2. Diffuse
3. Normal
4. EnvSpec
5. Subsurf

For each operator preserve:

`source -> producer -> transport -> consumer -> composition -> postprocess -> pixel`

Do not promote a global patch from one receiver family.

## Release discipline

The historical 1.45 binary-basis builder is frozen as release-reproduction infrastructure. New renderer development should not add more opaque RVA patch stages to that path.

The target state is a complete source build:

`git clone -> configure -> build -> addon64`

until then, release reproduction and source-first development remain explicitly separate.

## Tests

Do not use C/C++ `assert()` as the only test predicate in CI. Release builds normally define `NDEBUG`.

Tests must return a non-zero process exit code when a predicate fails, independently of build configuration.

## Evidence discipline

A new implementation result is not a pixel claim.

Record evidence first, then promote a finding only when the evidence supports the relevant status level. Failed or superseded experiments remain part of the history rather than being silently erased.
