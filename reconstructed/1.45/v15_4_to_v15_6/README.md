# V15.4–V15.6 reconstructed byte-exact deltas

Status: **RECONSTRUCTED / BYTE-EXACT / NOT ORIGINAL SOURCE**

The original historical V15.4/V15.5/V15.6 builder source was not present in the preserved runtime ZIPs. These builders were independently reconstructed from exact preserved parent/output addon binaries and the historical static audits.

They are deliberately stored under `reconstructed/` rather than `recovered-source/`.

## Verification

Each builder:

- hard-gates the exact parent size and SHA-256;
- validates every patch preimage;
- applies only the observed binary delta;
- verifies the exact historical output SHA-256;
- emits an audit recording every patched byte range.

Independent local reconstruction on 2026-09-23 produced byte-for-byte equality (`cmp` PASS) for all three stages.

### V15.4

Parent V15.3:
`4641d2f9d66e879cc86c05aa403a6f9fcfe96c5f9486c15cdae4339f15d50d68`

Output V15.4:
`4ddf2b270b09efcc57c4cf680ac0da78ebc3f0a5f58fd7d242b87b90aa6168cc`

Delta: one 7-byte formatter/thread-state bypass.

### V15.5

Parent V15.4:
`4ddf2b270b09efcc57c4cf680ac0da78ebc3f0a5f58fd7d242b87b90aa6168cc`

Output V15.5:
`bf552c8606e27ab1bf3535b27509e91d8692fe553993a0e80b767cd30c1dd283`

Delta: 19 two-byte displacement corrections implementing the BSS relocation documented by the historical audit.

### V15.6

Parent V15.5:
`bf552c8606e27ab1bf3535b27509e91d8692fe553993a0e80b767cd30c1dd283`

Output V15.6:
`9f112214a7cfc058775df26c58d4e8afaaa40fa22015613520bcaafcc9d5d21b`

Delta: re-enables formatter thread-state handling and relocates the remaining 15 uninit-path state references.

## Source-completeness impact

This closes the deterministic **binary-delta reproducibility** gap for V15.4→V15.6, but it does not recover the missing handwritten EnvSpec implementation. The main unresolved source gap remains the original V15.1-era `envcube_ext_v15_active_bind.c` / its predecessor implementation and earlier source-to-binary construction path.
