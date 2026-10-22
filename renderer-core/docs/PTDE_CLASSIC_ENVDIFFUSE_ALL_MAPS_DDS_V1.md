# PTDE Classic EnvDiffuse: source-complete *within available PTDE TPFs*

This is the PTDE **legacy** family `EnvDif_mXX_YYY`, **not** the 342-record `GI_EnvDif_*` packed-GI family. Parent native DSR identity registry: PR #500 (V360), scoped to m18 500 and 510 until additional exact DSR name+SHA identities are certified. The PTDE **asset sidecar is global** and never restricted to m18.

## Exact PTDE sources and outcome

From owner supplied `DSRRL_FINAL_EVIDENCE_PART02(2).zip`, sha256 `57314b36812dfa2867512052278a08dc0da8a41ea0414c1fbbe4ccdf45838076`:

- 9 decoded native TPF binders in `PTDE/PTDE_GI_ENV_PROBE_ASSETS/dvdbnd0/`, for m10 through m18.
- 1 native TPF in `PTDE/PTDE_GI_ENV_PROBE_ASSETS/dvdbnd1/`, with `EnvDif_def_000`.
- TPF entry count and original name table parsed, DDS header/type/size/6 cubemap faces authenticated for every `EnvDif` entry.
- Exact recovered counts: m10=9, m11=2, m12=3, m13=3, m14=5, m15=4, m16=1, m17=3, m18=8, default=1, **total 39**.
- 35 RGB24/BGR 32×32×6, 2 RGB24/BGR 64×64×6, 1 RGB24/BGR 8×8×6, 1 default BC1/DXT1 64×64×6 with 7 mips.
- `EnvDif_m18_510` is confirmed source RGB24 8×8×6 with all pixels `0x80` (128).

## Runtime-facing exact container

`DSRRL/EnvDiffuse/Legacy/PTDE_LEGACY_ENVDIFFUSE_ALL_MAPS_DDS_V1.bin`

SHA256 `3be05fffacfbc0af3f5a4124f0f79694c20480057b3a5c76470667ed712a6b47`, length `820208`. Little-endian header `<8sIIQQ>`: DSRLEDF1/version1/count39/index32/data5024. Each 128-byte record `<40s6HIQQ32s24s>` contains original ASCII name, map ID, row, dimensions, mip count, source format (1 RGB24/BGR, 2 BC1), cube flag, raw DDS byte offset/size/SHA. Payload is original DDS, *no conversion, fabricated alpha, resampling or gamma*.

The user artifact contains the exact binary, all 39 original DDS, `MANIFEST.json` with parent TPF and each DDS SHA, `INDEX.csv`, `build_ptde_legacy_envdiffuse_sidecar.py`, `verify_ptde_legacy_envdiffuse_sidecar.py`, and installation layout. Rebuild from owner source ZIP and independently verify 39 SHA.

## Evidence and activation boundaries

These 39 are **all legacy EnvDif entries in the 10 supplied extracted TPFs**. The complete original 1.5 GB PTDE BDT disks are not available here; global BDT source completeness remains OPEN.

The current V360 addon does **not yet read DSRLEDF1**. A complete bridge must add strict sidecar read/SHA validation, original DDS format/mip sampler handling, exact native DSR receiver/resource material mapping for each map and independent t12/t14 EnvSpec handling. Missing owner/PTDE donor/routing evidence => **stock DSR**. No PTDE pixel claim until in-game PTDE-vs-DSR results.

Canonical evidence: Supabase rev 14000 `renderer.ptde_legacy_envdiffuse_39_exact_dds_sidecar_20261022_v1`.
