# Late 1.45 recovered/reconstructed chain

Status: **BYTE-EXACT REPRODUCTION CHAIN / MIXED ORIGINAL + RECONSTRUCTED SOURCE**

This directory closes the historical binary lineage from the client-clean 1.45 build through the exact integrated basis later used by the public Nexus release materializer.

Chain:

- `41690c...` client-clean 1.45 -> `a5dc6f...` offline R11F RC1: 36-byte carrier-only transform; routing/DXBC unchanged.
- `a5dc6f...` -> `6df899...` P_Metal R11F Safe Operator Island RC2: receiver-first exact P_Metal island. Exact output is reconstructed from guarded source windows. Semantic shader implementation is recoverable from the original next-stage `REFERENCE_ORIGINAL_BUILD_PMETAL_R11F_EXACT_RECEIVER_V1.py` and the archived RC2 audit.
- `6df899...` -> `f869e5...`: telemetry-only activation V1.
- `f869e5...` -> `d02af3...`: telemetry-only V2.
- `d02af3...` -> `45c702...`: restores the proven V12 ordinary DifSpcBmp receiver ordinal gate `0..22` at common Diffuse PREPARE.
- `45c702...` -> `e15747...`: stage telemetry only.
- `e15747...` -> `db2e6547...`: original `BUILD_PMETAL_PTDE_ENVSPEC_RGBA_ISLAND_V1.py`; changes only dedicated DXBC 72/73/74 under guarded hashes.
- `db2e6547...` -> clean `3dcb50...` -> public `e44183...`: canonical release materializer and terminal-SAT patch on the Nexus review/main lanes.

The reconstructed RC2 windows are provenance-preserving exact materialization, **not a claim that the lost handwritten RC2 builder was recovered**. This keeps historical identity reproducible while the semantic implementation remains separately auditable.
