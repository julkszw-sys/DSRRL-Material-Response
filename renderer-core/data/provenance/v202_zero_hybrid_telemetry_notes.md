# DSRRL v2.0.2 final production P_Metal diagnostic build

**Purpose**: diagnose owner's confirmed visually stock DSR behavior (including Material Response) while production addon was loaded and MR draw_issued first-shot markers appeared.

- Identical production source/operator and DSR-only donor patch from branch HEAD 4d99bdfa6aff349aa5f859da03fdf7ebb91d08e4, itself directly derived from tagged final v2.0.2 b19a273f15872999a631e3feaa0a0dd6aea35e89.
- DSRRL_FULL_TELEMETRY_DEFAULT_ON=ON: runtime + effect counters (without D3D native-state verification).
- DSRRL_RELEASE_CLEANUP=OFF: restores nine otherwise compiled-out P_Metal hook counters and enables diagnostic codepaths without changing target operator, native source/receiver authorization or donor data. This diagnostic variant is **not** performance-equivalent to production.
- DSRRL_RESOURCE_EPOCH_SHARD_SYNC=ON, DSRRL_COMPANION_TLS_4WAY=ON, and physical PointLight, U/L, H3/SSS cuts kept identical to production.
- No compositor gain, blanket shader change, PointLight activation, PTDE runtime test, nor EXE disk modification.
- DSRRL BUILD_ID differentiates diagnostic from release. Startup plus every 300 present frames will log LIVE and effect matrix. Save complete ReShade log after entering game for at least several seconds in affected scene. The first LIVE log at startup has mostly zero counters; inspect later LIVE snapshots and PRE_UNLOAD too.
- Status CONSTRUCTION only until CI; RUNTIME and PIXEL OPEN / previous owner PIXEL FAIL.
