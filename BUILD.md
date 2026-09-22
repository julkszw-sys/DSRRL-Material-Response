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

- bypasses the five activation/development telemetry paths while preserving their functional continuations
- removes the telemetry strings
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

`13e722f9472e00c1922baecefe121bf1b7b9dd6129f1d2028d64568d74bb5ab7`

Version:

`1.45.0.0`

## Verify

```text
python tools/verify_release.py DSRRL_Material_Response_1.45.addon64
```

Construction PASS proves only that the binary has the declared release shape. Runtime liveness/bridge activation and pixel behaviour remain separate validation levels.
