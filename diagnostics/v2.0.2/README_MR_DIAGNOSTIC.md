# DSRRL 2.0.2 — FULL MR / LOCAL LIGHT DIAGNOSTIC

**Purpose:** find the shared lighting or routing fault that makes every armor type respond differently to light at one specific location. This is not a RoughCloth-specific fix. This diagnostic is derived from the published v2.0.2 source and **does not change** the MR shader/material operator, P_Metal, feature gates, resource-sidecar authorization, or `SYNC ON + TLS 4WAY`.

## Install / reproduce

1. Back up the production v2.0.2 addon and remove other DSRRL `.addon64` files. Load exactly **one** DSRRL addon.
2. Install `DSRRL_Restored_Lighting_v2.0.2_FULL_MR_DIAGNOSTIC.addon64` in the Dark Souls Remastered folder, keeping PTDE sidecars unchanged.
3. Load the save near the anomalous location. Reproduce the effect with **at least two different armor materials**, ideally metal and leather/cloth. Keep the camera/light positioning comparable.
4. Move a short distance outside the problematic spot and return; note approximately when this happens in the ReShade log.
5. Close the game and send the `ReShade*.log`, location/map and a description of how the highlight/shading changes. Screenshots from the affected and unaffected position may help localize the visible mismatch.

## Diagnostic markers

- `[DSRRL BUILD_ID]`, `[DSRRL CORE+ISLANDS 2.0.2]`: source version.
- `epochs=KEY_BUCKET_256`, `mode=4WAY_64SETS_256TOTAL_SYNC_ON`: synchronization unchanged.
- `[DSRRL MR MATERIAL]`: **all MR receiver materials**, not cloth-only, sampled/deduplicated with a hard cap of **512** records to avoid draw-time log flooding. Events: `owner_missing`, `decision_reject`, `decision_active`, `draw_issued`. Records receiver, MTD semantic/family hashes, raw-MTD/FLVER SHA prefixes, slot, material carrier, reason, route, certified operations, c100/c101 and PTDE specular power metadata.
- `[DSRRL MR ACT]`, `[DSRRL MR ACTIVE]`, `[DSRRL MR DIFFUSE ACT]` and `_RX`: activation stages and aggregated receiver counters.
- `[DSRRL ENVSPEC ...]`, `[DSRRL PMETAL ...]` and `[DSRRL POINTLIGHT ...]`: existing operator-resource/receiver diagnostic gates. Stock DSR PointLight remains stock — the addon PointLight islands remain physically cut.
- `[DSRRL HITCH FRAME]`, `[DSRRL STUTTER R43]`, `sampled_companion_tls_hit`: optional performance and cache context.

## Interpretation

- If different materials show an MR identity/rejection/dispatch change at the same location, investigate shared receiver/producer/transport first.
- If MR decisions and draw issuance stay consistent across that spot, the next suspect is **downstream or independent spatial light/probe/shadow behavior**; MR activity alone cannot certify PTDE pixel matching.
- The log does **not** automatically identify exact area coordinates or local light source IDs. Match the game location and screenshot with timestamps for source-to-consumer investigation.
- Full D3D state verification and logging alter frame timing. Use this binary **only for diagnostics**, not performance comparisons with production v2.0.2.
- Start close to the fault to avoid spending the 512-record bounded census on unrelated earlier materials. This limit restricts logging only; it never limits actual renderer work.

Public v2.0.2 remains the production release, unchanged.
