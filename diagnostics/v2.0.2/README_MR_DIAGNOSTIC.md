# DSRRL 2.0.2 FULL MR DIAGNOSTIC (SYNC ON + TLS 4WAY)

This is a diagnostic descendant of the published v2.0.2 build, **not a new release** and **not a new material-response fix**. MR, P_Metal, exact resource routing, fail-open and the physical-cut feature switches match the production configuration. QPC tracing, native D3D transaction verification, runtime/effect counters and bounded cloth tracing are enabled.

## Install

1. Back up your current v2.0.2 production addon and remove other DSRRL .addon64 files; use **one DSRRL addon only**.
2. Extract and copy `DSRRL_Restored_Lighting_v2.0.2_FULL_MR_DIAGNOSTIC.addon64` to the Dark Souls Remastered directory.
3. Keep your existing PTDE sidecars untouched. No PTDE resources are bundled.
4. Start the game. Visit the place and wear the cloth item that exhibits the difference. Rotate under light, revisit the same spot and also capture HemEnv and HemEnvLerp examples if possible.
5. Close the game normally and share the resulting `ReShade*.log` file with the affected cloth item and map. PTDE comparison screenshots are useful to assess the *visible residual* but cannot substitute for routing evidence.

## Log markers

- `[DSRRL CORE+ISLANDS 2.0.2]`, `[DSRRL BUILD_ID]` — exact version/source.
- `epochs=KEY_BUCKET_256`, `mode=4WAY_64SETS_256TOTAL_SYNC_ON` — sync retained.
- `[DSRRL MR ACT]`, `[DSRRL MR ACTIVE]`, `[DSRRL MR DIFFUSE ACT]`, `_RX` — owner/receiver, prepared and issued draw.
- `[DSRRL MR CLOTH]` — up to three diagnostic events per exact cloth MTD + receiver: `decision_active`, `decision_reject`, `draw_issued`. Fields include MTD identity, receiver, route, reason, authorization, c100, certified ops and raw-MTD digest prefix.
- `[DSRRL HITCH FRAME]`, `[DSRRL STUTTER R43]`, `sampled_companion_tls_hit` — frame QPC and TLS cache diagnostics.
- Material Response with a matching profile is **diffuse-domain-only**; a correct decision/draw does not prove cloth specular or pixel equivalence. If no cloth marker appears, check `_RX` counters and whether the item uses one of the four currently traced RoughCloth MTDs.

## Precautions

Full native-state verification and log instrumentation can change timings; **do not use this build for performance benchmarking against v2.0.2 production**. The addon does not edit `DarkSoulsRemastered.exe` on disk. No global lighting gains, texture replacements, shader material equations or receiver allowlists were expanded.

Use official production v2.0.2 for normal gameplay after collecting logs.
