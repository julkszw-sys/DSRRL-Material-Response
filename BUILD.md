# Building Material Response 1.45

## Requirements

- Python 3
- the exact development basis addon used for the 1.45 release step
- the exact PackedGI EnvSpec resource file

### Development basis

SHA-256:

`cfd1fe585710497a411adad8dc7edd9b46ae187133ee0625abeed2ab1ab96f09`

This is an intermediate addon binary from the development branch. The project was built incrementally, so the public 1.45 packaging step starts from this known-good basis rather than rebuilding every earlier renderer experiment from one monolithic source tree.

The basis binary is not committed to this repository. I can provide it to Nexus staff if it is needed for review.

### PackedGI resource

File:

`PTDE_GI_ENVSPEC_PACK_RGBA.bin`

Size:

`33,619,968 bytes`

SHA-256:

`c16c3fd75bcf34f3cc075da6da1ad10c9440ee4a3ca580fe7f74d07a2ce4eac3`

The PackedGI data is game-derived and is not stored in this source repository.

## Build command

From the repository root:

```text
python tools/build_release.py \
  --base "PATH/TO/development_basis.addon64" \
  --pack "PATH/TO/PTDE_GI_ENVSPEC_PACK_RGBA.bin" \
  --out "DSRRL_Material_Response_1.45.addon64"
```

The builder reads the exact loader bytes from `reference/loader_bytes.hex` automatically.

## Verify the result

```text
python tools/verify_release.py DSRRL_Material_Response_1.45.addon64
```

Expected output:

- size: `1,803,264 bytes`
- SHA-256: `41690c6212157eb772ae0c75c055d0bb7a842709f65f02c3689b87714151e1f7`
- file version: `1.45.0.0`

The builder aborts if the development basis, PackedGI resource, loader bytes or generated release do not match the expected identities.

## What the release builder changes

The 1.45 release step:

- verifies the PackedGI resource
- installs the local external-resource loader
- enables the guarded resource-ready path
- removes development telemetry from the client build
- updates the public addon name and version metadata
- removes the embedded copy of the PackedGI corpus
- keeps the existing renderer bridge logic intact
- checks the final output size and SHA-256

## Loader source

`src/envspec_loader.c` is a readable C implementation of the same loader contract used by the release.

The shipping binary uses the fixed Win64 implementation represented by `reference/loader_bytes.hex`, so the C file is provided for readability rather than as a claim of byte-for-byte compiler output.
