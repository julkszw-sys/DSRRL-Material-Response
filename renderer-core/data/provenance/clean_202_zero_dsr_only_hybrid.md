# DSRRL 2.0.2 clean-source zero-descriptor and donor hybrid

Parent: `410e4591da9172ffb15e766754b4345286f04691` (no later PR/branch ancestry).

- Retail 0x14020E9D0 generic FLVER descriptor's synthetic zero is not
  a LightBank source. Do not publish it to P_Metal material-key state.
- Invalidate a P_Metal material only after exact native source validation.
- Never consume process-last `latest_hook_source` for the current material.
- Original PTDE P_Metal donor rows remain byte-identical. Exactly 195 DSR-only
  LightBank records are authorized by original bank structure identity:
  3 extra row64 (m14/s14/m18) + 192 default/m99/s99.
  EnvDiffuse and EnvSpec RGBM are separately authored in original DSR.
- PointLight ten gameplay banks are unchanged. Original DSR-only
  default/m99 128 rows are matched by complete original PARAM table identity
  and fed to the existing PointLight operator.
- The exact original DSR source data headers are preserved as zlib blobs
  solely to keep the tree small; GitHub CI verifies their uncompressed SHA256
  before configuring Windows x64 addon.
- This is a construction experiment until in-game owner runtime and pixel
  behavior are validated. No EXE modifications.
