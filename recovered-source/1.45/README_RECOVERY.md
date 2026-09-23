# Material Response 1.45 — recovered original source

This directory preserves the original source/build files recovered from the
historical DSRRL V12, V15.7 and client-1.45 packages.

## Integrity

The byte-exact recovered core is stored in:

`archive/core/DSRRL_Material_Response_1.45_CORE_SOURCE.tar.xz.b64.part00..09`

Decoded archive:

- size: `56,232 bytes`
- SHA-256: `f701f11d32fa3a993f787e8c6abe9a14e33267c2e3d94270e50ce32a7feaf25a`

Materialize it with:

```text
python recovered-source/1.45/materialize_recovered_source.py
```

The archive contains exact recovered originals including:

- `v12/asset_integrated_v12.c`
- `v12/asset_stubs_v12.s`
- `v12/build_v12.py`
- `v15_7/build_v15_7.py`
- `client145/build_client145.py`
- `client145/build_client145_final.py`
- recovery manifest and source hashes.

## Completeness status

**PARTIAL / recovery-in-progress.**

This is materially better than the previous 1.45 review branch because the
V12 renderer bridge source and the V15.7/client builders are preserved as
original source files rather than treating the final addon as the only source.

It is **not yet claimed as a source-complete from-scratch lineage** for every
intermediate EnvSpec stage. The remaining recovery target is the original
source-level V15.1 / V15.5–V15.6 EnvSpec BSS/thread-state implementation.
Until that gap is closed, do not replace the source-completeness status with
COMPLETE.

The public 1.45 shipping binary remains the oracle for binary/runtime
comparison; recovered-source status does not imply pixel equivalence.
