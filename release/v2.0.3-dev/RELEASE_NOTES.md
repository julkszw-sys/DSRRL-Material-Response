# DSRRL Restored Lighting — v2.0.3-dev

**Development prerelease / owner-confirmed restored behavior. Not a stable release.**

This is a **byte-identical promotion** of the corrected Windows x64 ReShade addon the owner confirmed visually works after restoring the final 2.0.2 native LightBank fallback. The published addon file is renamed for distribution only. No shader, constant-buffer, native hook, source/binary or sidecar logic is modified at promotion time.

- Runtime source: [`6a73033f6275f88487effeb1e34bbbdd7a6a3a88`](https://github.com/julkszw-sys/DSRRL-Material-Response/commit/6a73033f6275f88487effeb1e34bbbdd7a6a3a88), descended from tagged production v2.0.2 source `b19a273f15872999a631e3feaa0a0dd6aea35e89`.
- Reproducible original Windows build: [run 37913412011](https://github.com/julkszw-sys/DSRRL-Material-Response/actions/runs/37913412011), artifact `11607274698`. Build and native regression tests PASS.
- Unchanged addon SHA-256: `aa7c44b7cc79e36b386f2a7589432160b59dd739b5c56708aaf3ac0e7eb47c36`; size `1,846,784` bytes, Windows x64 PE64 DLL.
- Internal binary build flavor remains `v2_0_2_production_zero_dsr_only_hybrid`. `v2.0.3-dev` is the prerelease/distribution version; do not misidentify its embedded version as newly compiled 2.0.3.
- Corrected source restores `pmetal_env_source_runtime::latest() -> latest_hook_source(out)` and keeps the native synthetic-zero selector exclusion plus 195 exact original DSR-only P_Metal LightBank donor records.
- Original Material Response, Diffuse/Normal/SpecRGB sources and production SYNC ON / TLS 4-way, and stock cut PointLight/UpperLower/HemDir3/SSS policy remain unmodified.
- Owner confirms visual effects are restored compared to the failing candidate. **PTDE pixel-equivalence not yet proved**, and no improvement claim is made relative to verified PTDE pixel target.
- Final product is one `.addon64`. Never install simultaneously with a different DSRRL addon. Game executable stays unchanged on disk.

The previous stable `v2.0.2` release/tag and its original binary are not replaced.
