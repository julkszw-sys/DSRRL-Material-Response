# Building Material Response 1.45 clean release

## Requirements

- Python 3
- the exact integrated 1.45 basis addon

No EnvSpec/PackedGI resource is required. EnvSpec/cubemap replacement is intentionally disabled in this release.

## Integrated basis

Expected SHA-256:

`db2e6547b5fb5516d4ad6559173e66421315d1635ca0c08e8141b0de2f57f966`

The project was developed incrementally, so the public clean-release step starts from this known integrated 1.45 addon and performs an auditable hardening transformation.

## Build command

From the repository root:

```text
python tools/build_release.py \
  --base "PATH/TO/DSRRL_Material_Response_1.45.integrated.addon64" \
  --out "DSRRL_Material_Response_1.45.addon64"
```

## What the clean builder changes

The release builder:

- bypasses the one-shot SpecRGB/Subsurf/Normal/Diffuse activation telemetry paths while preserving their functional continuations
- removes their telemetry strings
- disables the inherited startup shader-count telemetry log
- skips registration of the periodic counter/status telemetry callback and hard-disables the callback entrypoint as a second guard
- preserves safety/error/fail-open logging
- skips EnvSpec external-loader initialization
- forces the EnvSpec resource pointer to null at the bridge gate so the path fails open before substitution work
- clears the now-unreferenced 1,501-byte injected EnvSpec loader cave
- removes EnvSpec from public addon metadata
- preserves Material Response, PTDE SpecRGB, Subsurf and equipment Normal/Diffuse logic
- recomputes the PE checksum
- verifies the final output identity and all release invariants

## Expected output

File:

`DSRRL_Material_Response_1.45.addon64`

Size:

`1,803,264 bytes`

SHA-256:

`3dcb50bee7d4a1ffcb47c2e9d116cbad5da322f6719e3cf2ecdbe6846db63000`

PE checksum:

`0x001BE0F4`

Version:

`1.45.0.0`

## Verify

```text
python tools/verify_release.py DSRRL_Material_Response_1.45.addon64
```

Construction PASS proves only that the binary has the declared release shape. Runtime liveness/bridge activation and pixel behaviour remain separate validation levels.
