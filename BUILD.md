# Building the exact Material Response 1.45 Nexus release

## Requirements

- Python 3
- the exact integrated 1.45 basis addon

No EnvSpec/PackedGI resource is required. EnvSpec/cubemap replacement is intentionally disabled in this release.

## Integrated basis

Expected SHA-256:

`db2e6547b5fb5516d4ad6559173e66421315d1635ca0c08e8141b0de2f57f966`

This basis is the integrated 1.45 lineage immediately before release hardening. The project was developed incrementally, so exact shipping reproduction starts from this known integrated artifact rather than pretending that the historical addon was produced by one monolithic compiler invocation.

## Deterministic build stages

### Stage 1 — clean release hardening

`tools/build_release.py` first:

- bypasses one-shot SpecRGB/Subsurf/Normal/Diffuse activation telemetry while preserving functional continuations
- removes their telemetry strings
- disables the startup shader-count telemetry log
- skips registration of the periodic counter/status callback and makes its entrypoint inert
- preserves safety/error/fail-open logging
- skips EnvSpec external-loader initialization
- forces the EnvSpec resource pointer to null so the replacement path fails open
- clears the unreferenced 1,501-byte EnvSpec loader cave
- removes EnvSpec from public addon metadata
- preserves the integrated renderer/shader payload
- recomputes the PE checksum

That exact intermediate is:

- SHA-256: `3dcb50bee7d4a1ffcb47c2e9d116cbad5da322f6719e3cf2ecdbe6846db63000`
- Size: `1,803,264 bytes`
- PE checksum: `0x001BE0F4`

### Stage 2 — final Nexus P_Metal terminal SAT delta

The builder then invokes the same implementation exposed separately as:

`tools/patch_pmetal_terminal_sat.py`

It changes only the final RGB output write in embedded DXBC indices 33, 34 and 35:

`MOV o0.xyz, ...` -> `MOV_SAT o0.xyz, ...`

It then recomputes each affected DXBC checksum and the PE checksum.

Exact resulting shader hashes:

- DXBC 33: `ee2511b2c3c6a822c921ad0ca5ef9eaf78b05d1992ce88851b5ecae95601c872`
- DXBC 34: `3cb53c033f61ef7664be097373c87d1a5f0933c2d3342feb6c50a63b5e3b49dd`
- DXBC 35: `71b973e36cb2ebbabc05455c1f882653ad39532644051d7d1e2e045874d427d3`

The release also retains the integrated dedicated DXBC 72/73/74 byte payloads with hashes documented by the verifier; active PTDE EnvSpec resource substitution itself is disabled by the release cut.

## Build command

From the repository root:

```text
python tools/build_release.py \
  --base "PATH/TO/DSRRL_Material_Response_1.45.integrated.addon64" \
  --out "DSRRL_Material_Response_1.45.addon64"
```

## Expected final output

`DSRRL_Material_Response_1.45.addon64`

- Size: `1,803,264 bytes`
- SHA-256: `e44183ef10fc7921f7741eb16d54ec30e814f580421ac6c83be18b5308b73342`
- PE checksum: `0x001BC666`
- Version: `1.45.0.0`

## Verify

```text
python tools/verify_release.py DSRRL_Material_Response_1.45.addon64
```

For reviewers who already have the clean `3dcb50...` intermediate, the final shipping delta can be reproduced independently:

```text
python tools/patch_pmetal_terminal_sat.py \
  DSRRL_Material_Response_1.45.clean-intermediate.addon64 \
  DSRRL_Material_Response_1.45.addon64
```

Construction PASS proves exact binary shape/identity only. Runtime liveness, bridge activation and pixel behaviour remain separate validation levels.
