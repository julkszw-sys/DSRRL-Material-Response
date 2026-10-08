# DSRRL — Release v2.0.0

This release preserves the **exact binary** from owner-tested working R43 audit hardening build #229, not a new compilation. This intentionally retains the P_Metal source fallback from the last working variant. The PR #273 keyed-source-only change has been runtime-falsified by owner A/B and is **not** included.

- Addon SHA256: `37809b9d18fdbd690c7d4e6ad028692f15203d8f95b608a5c4fef4cda94ecd81`
- Source commit: `f2704ce1f10ae44c6b474a45e94de0b05f7698ac`
- Original [pinned Windows CI run](https://github.com/julkszw-sys/DSRRL-Material-Response/actions/runs/37721493541).
- ReShade 6.8.0.1 (64-bit); integrated single `.addon64`.
- Owner-verified: restored original variant works with their PTDE equipment DDS and P_Metal EnvSpec assets.
- Active: Material Response and exact equipment texture / P_Metal EnvSpec bridges under their narrow receiver and resource rules.
- Intentionally stock DSR: PointLight, Upper/Lower, HemDir3 and Subsurface (physically cut at build time).
- Known asset fail-open: absent `wp_a_1535_s` PTDE SpecRGB sidecar.
- Limitation: owner acceptance is not comprehensive PTDE pixel equivalence across all maps, receiver families, or equipment. Performance remains unverified.

**Important:** internal version and logs can still say `2.0.0-dev`: altering the compiled string would change the owner-validated binary identity. The public package version is **Release v2.0.0**.

The public distribution contains no extracted proprietary PTDE resources. Users must provide lawful PTDE-sourced DDS and PackedGI resources separately. The owner-local package contains owner-provided PackedGI but no equipment DDS.

See [INSTALL](./INSTALL.md) and [manifest](./RELEASE_MANIFEST.json) for hashes, paths and verification.
