# Material Response pre-V12 source recovery

This recovery lane closes the historical addon chain **byte-for-byte from clean Material Response 1.0 (`73fedd...`) forward to build52 (`deb7dbba...`)**, which is the exact predecessor chain feeding the already recovered V2→V12 and V15.1→V15.7 lineage.

The recovery deliberately distinguishes three classes:

1. **Preserved original source/builders** — strongest provenance.
2. **Semantic reconstruction** — source transformation is reconstructed from audit/ABI/DXBC evidence and reproduces the historical bytes exactly.
3. **Guarded exact reconstruction** — historical binary delta is lifted into an auditable source patcher. This proves deterministic historical reproduction but is **not** claimed to be the lost handwritten builder.

## Major closure results

- V13 companion: original `SOURCE_V13_PTDE_DONOR.cpp` recovered. Recovered Clang/LLD recipe reproduces the historical companion SHA `1992ab7c...b365` exactly; only the historical COFF timestamp needs restoration.
- V13 main/fusion and V14 shader stage: exact guarded reconstruction to `498960ef...`, `9f98b52b...`, and `828c0138...`.
- MR1.1 FULL_SAFE: exact `bb23eefb...` from V14.
- MR1.2 FULL_SAFE internal-t10: exact `fea208ab...`; historical side branch.
- MR1.2R fail-open: semantic three-receiver DXBC transformation from MR1.1; exact `6711b90d...`.
- Late MR1.3 → Spec Bridge RC → telemetry: exact `d2917313...` → `5e26eb50...` → `633d50b9...`.
- Telemetry → Lit → Mul → MulLit → FULL24: semantic DXBC reconstruction, all exact.
- FULL24 → plain-route: exact reconstructed guarded stage `ad23449c...`.
- SPEC_ONLY: preserved original builder plus reconstructed semantic receiver inputs; exact `5794623c...`.
- build52 Subsurf: preserved original transformer + original builder; exact `deb7dbba...`.

## Remaining source-completeness gap

The earliest unresolved exact lineage join is now **the origin of clean MR1.0 `73fedd95873fc08020e17b266d75228b517685ac519447fd0bfb05f024569652`**.

This is narrower than “Material Response source missing”: an earlier full V2.9.1 source package has been recovered, including `addon.cpp`, `selector_hook.asm`, generated shader payload header, c100 donor registry, SHA256 core and Windows build script. What remains OPEN is proving/reconstructing the exact lineage from that source-era implementation / subsequent V2.28 stages to the specific clean MR1.0 release binary.

Therefore **source_complete remains false** and the project-wide `NO_RC_OR_RELEASE_WITHOUT_SOURCE_COMPLETE_GIT_COMMIT` PROTECT remains in force for any new reconstructed release lineage.
