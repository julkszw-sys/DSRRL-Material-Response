# V15.1 -> V15.7 exact recovery chain

The historical V15.3/V15.4/V15.5/V15.6 builder source was not retained in the archived packages. These four builders were reconstructed from the original static audits and independently verified against the archived addon binaries.

Each reconstructed stage is byte-identical to history:

- V15.3 `4641d2f9d66e879cc86c05aa403a6f9fcfe96c5f9486c15cdae4339f15d50d68`
- V15.4 `4ddf2b270b09efcc57c4cf680ac0da78ebc3f0a5f58fd7d242b87b90aa6168cc`
- V15.5 `bf552c8606e27ab1bf3535b27509e91d8692fe553993a0e80b767cd30c1dd283`
- V15.6 `9f112214a7cfc058775df26c58d4e8afaaa40fa22015613520bcaafcc9d5d21b`
- V15.7 `cfd1fe585710497a411adad8dc7edd9b46ae187133ee0625abeed2ab1ab96f09`

V15.7 is generated using the recovered original `build_v15_7.py`; starting from the reconstructed V15.6, its output was independently compared byte-for-byte to the archived historical V15.7 addon.

## Causal crash closure

- V15.3: disables active PRE/PREPARE/POST transaction while preserving V15.1 state layout.
- V15.4: bypasses only per-thread A/B formatter semantic store.
- V15.5: relocates conflicting extension statics from V12-owned `0x112300..0x112316` to safe `0x112260..0x112276`.
- V15.6: restores per-thread A/B store and also relocates unload/uninit saved-prolog references.
- V15.7: re-enables the three active V15.1 transaction wrapper calls.

Thus the startup crash was a static `.v13d` ownership collision, not an inherent failure of the per-thread semantic or t12/t14 transaction design.