# EnvSpec V15.1 -> V15.7 exact reconstruction

Status: **EXACT HISTORICAL BINARY-DELTA RECONSTRUCTION PASS**  
Source-complete status: **NO**

This directory closes the previously unresolved V15.3/V15.4/V15.5/V15.6/V15.7 transition chain from the archived V15.1 runtime.

Input V15.1:
`7f10f908d598f8d6b952e2c78bce6eb97a8b84a297f956a069c439b7977b65cc`

Exact reconstructed outputs:

- V15.3 `4641d2f9d66e879cc86c05aa403a6f9fcfe96c5f9486c15cdae4339f15d50d68`
- V15.4 `4ddf2b270b09efcc57c4cf680ac0da78ebc3f0a5f58fd7d242b87b90aa6168cc`
- V15.5 `bf552c8606e27ab1bf3535b27509e91d8692fe553993a0e80b767cd30c1dd283`
- V15.6 `9f112214a7cfc058775df26c58d4e8afaaa40fa22015613520bcaafcc9d5d21b`
- V15.7 `cfd1fe585710497a411adad8dc7edd9b46ae187133ee0625abeed2ab1ab96f09`

Every reconstructed stage matches the corresponding owner archival addon byte-for-byte by SHA-256.

## Recovered mechanism

V15.3 restores PRE/PREPARE/POST to the V12 targets while preserving V15.1 state/init/formatter code.

V15.4 bypasses only the V15.1 per-thread A/B semantic-store entry.

V15.5 fixes the confirmed static-state collision by relocating V15-owned references from the V12-live `0x112300..0x112316` range to `0x112260..0x112276`.

V15.6 re-enables the original V15.1 thread-state store and relocates the previously missed uninit/unload references to the same safe BSS range.

V15.7 reactivates only the three exact-slot EnvSpec draw callsites:

- PRE -> `envcube_pre_wrapper`
- PREPARE -> `envcube_prepare_wrapper`
- POST -> `envcube_post_wrapper`

## Important limitation

This reconstruction proves the exact historical V15.1 -> V15.7 binary lineage and closes the BSS/thread-state patch history. It does **not** recreate the still-missing handwritten V15.1 C source body. The remaining source-complete blocker is therefore narrower: recover/reconstruct the V15.1 implementation itself from its exact binary plus preserved semantic/audit evidence, and close the pre-V12 binary-only predecessor chain.
