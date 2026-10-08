# DSRRL Restored Lighting v2.0.2

**Release scope:** Windows x64 ReShade API 20, Dark Souls Remastered, one `.addon64`, R43 renderer/operator configuration.

## What's new
- **SYNC ON in production:** companion-resource TLS invalidation uses 256 resource-key buckets instead of one global cache epoch. Resource-view create/destroy no longer invalidates unrelated TLS cache buckets.
- **4WAY companion TLS lookup:** 64 sets × 4 ways, still **256 entries per thread**. This reduces direct-map conflicts and repeated reads of the authoritative resource registry under its mutex without increasing the number of cached COM references.
- **Exact-identity behavior preserved:** resource-view lifetime/ABA invalidation, ambiguous view quarantine, per-device teardown, original global authoritative registry lock and fail-open fallback.
- Retains the **R43 P_Metal and generic Material Response** operator routing, exact PTDE resource sidecars and tested runtime feature policy.
- **Release cleanup:** removes QPC stutter profiler, per-frame hitch trace, sampled TLS hit/mutex telemetry and experimental sidecar DDS pooling from the production build. Leaves runtime activation markers and non-diagnostic provenance.

## Measured experimental lineage
In the **owner-tested development build** (`2b9f5b55e1d88807b2821eb975dccbca729528af`, SYNC ON + 4WAY + diagnostic profiler), sampled companion-TLS hits rose to **99.98%** (1,224,064 sampled hits / 256 sampled mutex fallbacks) versus approximately **96.82%** on the earlier SYNC ON 1WAY run. After first Material Response activation, 32 five-second windows covering 9,606 Present calls recorded a maximum frame interval of 40.052 ms and no intervals over 50 ms. The owner described gameplay as very smooth.

**Evidence limits:** runs were not matched-route A/B, the diagnostic and production binaries differ in instrumentation and layout, and sampled counters aggregate in batches of 128 per thread. These are evidence of runtime effectiveness of the tested experimental configuration, **not** proof of universal frametime or PTDE-visible pixel equivalence. Production v2.0.2 requires its own smoke test.

## Renderer policy / unchanged
- DSR remains the renderer/runtime. `DarkSoulsRemastered.exe` is not modified on disk.
- P_Metal EnvSpec/EnvDiffuse PTDE source and exact receiver/material routes retained.
- Generic Material Response for authenticated HemEnv/HemEnvLerp routes retained.
- Existing physically cut PointLight addon islands stay stock DSR PointLight.
- Upper/Lower, HemDir3 and visible Subsurface addon bridges stay disabled; stock DSR paths preserved.
- No global removal of TAA, PBL, SSS, bloom or native specularity.
- Incompatible, missing or non-authoritative PTDE resource owners fail open to stock DSR.

## Installation
1. Use a clean ReShade 6.8.x installation for **64-bit** Dark Souls Remastered.
2. Back up and remove older DSRRL `.addon64` variants from the game directory. Load **exactly one** DSRRL addon.
3. Place `DSRRL_Restored_Lighting_v2.0.2.addon64` alongside `DarkSoulsRemastered.exe`.
4. Keep your existing owner-supplied exact PTDE resource sidecars in their expected DSRRL folders. PTDE assets are **not included** in the public core package.
5. Start the game; confirm ReShade loads the DSRRL addon and prints `version=2.0.2` and `epochs=KEY_BUCKET_256`, `mode=4WAY_64SETS_256TOTAL_SYNC_ON`.

## Integrity and support
- See `SHA256SUMS.txt` and `DSRRL_v2.0.2_PROVENANCE.json` for build identity.
- If the runtime fails to activate expected routes, verify sidecar names/material/receiver identities before editing lighting values.
- Runtime activation, visual improvement and PTDE pixel equivalence are distinct validation levels. Exact all-scene PTDE pixel equivalence remains **OPEN**.
