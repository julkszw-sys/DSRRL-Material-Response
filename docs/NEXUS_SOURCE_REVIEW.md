# Nexus source review — Material Response 1.45

This branch exists specifically for Nexus Mods manual review of the compiled ReShade addon used by **Dark Souls Remastered – Restored Lighting**.

## File under review

`DSRRL_Material_Response_1.45.addon64`

- Size: `1,803,264 bytes`
- SHA-256: `3dcb50bee7d4a1ffcb47c2e9d116cbad5da322f6719e3cf2ecdbe6846db63000`
- Type: 64-bit ReShade addon / PE DLL

The clean 1.45 release intentionally has the experimental PTDE EnvSpec/cubemap replacement disabled.

## Source layout

There are two separate review paths because the addon was developed incrementally rather than as one monolithic clean-room rewrite.

### 1. Inspectable C++ runtime source

`review-src/` contains a buildable C++/ReShade API 20 snapshot of the runtime state/routing layer used during development. It exposes the relevant in-process renderer behaviour for review:

- ReShade addon registration and renderer callbacks
- D3D11 resource and resource-view tracking
- pixel-pipeline tracking
- per-command-list state
- generation-safe resource identity
- route/fail-open contracts
- draw transaction / restore accounting
- no networking, downloader, process launcher, service, driver or persistence code

This snapshot is taken from public development commit:

`35095ba189b3bc3ed5c53e72488824a914bca0f6`

It is provided for source inspection and for building a review addon. It is **not claimed to be byte-identical to the shipping 1.45 addon**, because the shipping addon was produced through an incremental binary-development lineage.

### 2. Exact shipping-binary reproduction step

The exact clean 1.45 shipping file is deterministically produced by:

- `tools/build_release.py`
- verified by `tools/verify_release.py`

from the exact integrated 1.45 basis:

`db2e6547b5fb5516d4ad6559173e66421315d1635ca0c08e8141b0de2f57f966`

The integrated basis is an intermediate development addon and is not committed to the public repository. It can be supplied directly to Nexus staff for review.

Running the release builder over that exact basis produces:

`3dcb50bee7d4a1ffcb47c2e9d116cbad5da322f6719e3cf2ecdbe6846db63000`

See `BUILD.md` for the exact release materialization command.

## Build the public C++ review target

Requirements:

- Windows 10/11 x64
- Visual Studio 2022 with **Desktop development with C++**
- CMake 3.20+
- Git
- Python 3 only if reproducing/verifying the exact clean-release transformation
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

This target is for source-code inspection and build verification. It is deliberately separated from the exact release hash claim above.

## Reproduce the exact clean 1.45 release

If Nexus staff has the integrated basis file whose SHA-256 is:

`db2e6547b5fb5516d4ad6559173e66421315d1635ca0c08e8141b0de2f57f966`

run:

```bat
python tools\build_release.py ^
  --base "PATH\TO\DSRRL_Material_Response_1.45.integrated.addon64" ^
  --out "DSRRL_Material_Response_1.45.addon64"

python tools\verify_release.py DSRRL_Material_Response_1.45.addon64
```

Expected SHA-256:

`3dcb50bee7d4a1ffcb47c2e9d116cbad5da322f6719e3cf2ecdbe6846db63000`

## Security-relevant behaviour

The addon runs inside the game process because that is required by the ReShade addon API and renderer bridge.

Expected renderer behaviour includes:

- ReShade callbacks
- D3D11 resource/view tracking
- shader/pipeline identity checks
- temporary draw-scoped resource/state substitution in supported bridges
- restoration of the original state after the draw
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

See also `SECURITY.md` and `docs/NEXUS_REVIEW.md`.
