# Nexus source review — Material Response 1.45

This branch exists specifically for Nexus Mods manual review of the exact compiled ReShade addon uploaded for **Dark Souls Remastered – Restored Lighting**.

## Exact shipping identity

`DSRRL_Material_Response_1.45.addon64`

- Size: `1,803,264 bytes`
- SHA-256: `e44183ef10fc7921f7741eb16d54ec30e814f580421ac6c83be18b5308b73342`
- PE checksum: `0x001BC666`
- Type: 64-bit ReShade addon / PE DLL

The Nexus ZIP containing it is:

- SHA-256: `2439d24644a0dc5bef6e29b1d980bab5b421a08c6bc71fca93d4c88859ad0893`
- Size: `244,154 bytes`

The earlier `3dcb50...` hash is only the deterministic clean intermediate before the final terminal-SAT delta.

## Source/reproduction layout

The addon was developed incrementally. For review, this branch therefore exposes two complementary layers instead of making a false byte-identical-source claim.

### 1. Inspectable C++ runtime source

`review-src/` contains a buildable C++ / ReShade API 20 snapshot of the runtime state/routing layer used during development. It exposes the security-relevant in-process behaviour:

- ReShade addon registration and renderer callbacks
- D3D11 resource and resource-view tracking
- pixel-pipeline tracking
- per-command-list state
- generation-safe resource identity
- route/fail-open contracts
- draw transaction / restore accounting
- no networking, downloader, process launcher, service, driver or persistence code

This review target is for code inspection and runtime architecture review. It is not represented as a byte-identical compiler source for the incrementally developed shipping PE.

### 2. Exact shipping-binary reproduction source

The exact shipping file is deterministically materialized by:

- `tools/build_release.py`
- `tools/patch_pmetal_terminal_sat.py`
- verified by `tools/verify_release.py`

The release builder begins from the exact integrated 1.45 basis:

`db2e6547b5fb5516d4ad6559173e66421315d1635ca0c08e8141b0de2f57f966`

It first produces the EnvSpec-disabled clean intermediate:

`3dcb50bee7d4a1ffcb47c2e9d116cbad5da322f6719e3cf2ecdbe6846db63000`

Then the public terminal-SAT materializer changes only the final RGB write in embedded DXBC 33/34/35, recalculates their DXBC checksums and the PE checksum, producing the exact Nexus file:

`e44183ef10fc7921f7741eb16d54ec30e814f580421ac6c83be18b5308b73342`

The SAT materializer was independently checked against the uploaded Nexus binary and reproduces it byte-for-byte from the clean intermediate.

## P_Metal terminal SAT source-level statement

The final release delta is deliberately narrow:

`MOV o0.xyz, source`

becomes:

`MOV_SAT o0.xyz, source`

at the terminal RGB output instruction of embedded DXBC 33, 34 and 35. Alpha is not modified by this delta.

Exact final shader hashes are listed in `RELEASE_HASHES.md` and enforced by `tools/verify_release.py`.

This source statement does not assert a broader material scope than the runtime routing proves; it documents the exact binary mutation present in the Nexus file.

## EnvSpec status

The integrated lineage still contains historical EnvSpec-related shader/resource work, but the **shipping release path disables PTDE EnvSpec/cubemap replacement**:

- external loader initialization is skipped
- the resource gate is forced to null/fail-open
- the injected loader cave is cleared
- no PackedGI EnvSpec sidecar is required

## Build the public C++ review target

Requirements:

- Windows 10/11 x64
- Visual Studio 2022 with **Desktop development with C++**
- CMake 3.20+
- Git
- ReShade source pinned to commit:
  `3645e3025d1d98a90e318278858931f034d5d1f6`

From a Visual Studio x64 Developer Command Prompt:

```bat
git clone https://github.com/julkszw-sys/DSRRL-Material-Response.git
cd DSRRL-Material-Response
git checkout nexus-review/material-response-1.45-source

git clone https://github.com/crosire/reshade.git _reshade
git -C _reshade checkout 3645e3025d1d98a90e318278858931f034d5d1f6

cmake -S review-src -B build-review -A x64 -DRESHade_INCLUDE_DIR="%CD%\_reshade\include"
cmake --build build-review --config Release
ctest --test-dir build-review -C Release --output-on-failure
```

Expected review addon:

`build-review\Release\DSRRL_Nexus_Source_Review.addon64`

This target exists for source inspection; do not compare its SHA-256 to the shipping PE.

## Reproduce the exact Nexus release

With the exact integrated basis whose SHA-256 is:

`db2e6547b5fb5516d4ad6559173e66421315d1635ca0c08e8141b0de2f57f966`

run:

```bat
python tools\build_release.py ^
  --base "PATH\TO\DSRRL_Material_Response_1.45.integrated.addon64" ^
  --out "DSRRL_Material_Response_1.45.addon64"

python tools\verify_release.py DSRRL_Material_Response_1.45.addon64
```

Expected SHA-256:

`e44183ef10fc7921f7741eb16d54ec30e814f580421ac6c83be18b5308b73342`

## Security-relevant behaviour

The addon runs inside the game process because that is required by the ReShade addon API and renderer bridge.

Expected renderer behaviour includes:

- ReShade callbacks
- D3D11 resource/view tracking
- shader/pipeline identity checks
- temporary draw-scoped resource/state substitution in supported bridges
- restoration of original state after the draw
- fail-open to stock DSR when route/resource/material identity is not verified

The addon does not intentionally:

- contact remote servers
- perform HTTP requests
- download or execute other programs
- launch external processes
- install services or drivers
- create scheduled tasks or autorun persistence
- collect credentials or personal data
- modify `DarkSoulsRemastered.exe` on disk

See also `SECURITY.md`, `docs/NEXUS_REVIEW.md` and `docs/EXACT_NEXUS_RELEASE_DELTA.md`.
