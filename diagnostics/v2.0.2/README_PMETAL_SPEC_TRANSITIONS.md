# v2.0.2 P_Metal SpecRGB transition diagnostic

This diagnostic build retains the production v2.0.2 R43 renderer and SYNC ON + TLS4WAY. It logs whether P_Metal prepares the PTDE operator or falls back to stock DSR, sampled for the same armor, FLVER slot and receiver. It does not alter shaders, resources or the authorization gate.

Install exactly one DSRRL addon: `DSRRL_v2.0.2_PMETAL_SPEC_TRANSITIONS.addon64`. Retain all existing PTDE sidecars. At the affected location, hold your character stationary, rotate the camera for 10 to 20 seconds, stop, then repeat using another armor. Send the ReShade log with the location and approximate moment of the visual change.

Look for `[DSRRL PMETAL SPEC TRANSITION]` with `PTDE_PREPARED` versus `STOCK_FAILOPEN`. Compare the same `owner_sha0`, `slot`, `rx`, stock `t1`, logical `hash`, `registry`, `ambiguous`, `snap`, `allowed`, `companion`, `probeA/B`, `t12/t14`, `bankA/rowA` and `beta` across the observation. Samples are bounded at 4096 lines, with immediate status/probe/beta transitions and otherwise one per 200ms. `ms` is a monotonic timestamp, not a frame index.

A missing registered view needs resource identity diagnosis, not suppression of fail-open. The diagnostic build is for routing analysis, not performance testing or proof of PTDE pixel equivalence.

## v2.0.2 row-origin update
Owner runtime log `ReShade(20261008-193204).log` showed stable exact material slot and PTDE SpecRGB/probe while `rowA` changed `32 -> 22 -> 23 -> 22` under camera-only rotation. That is **not** a demonstrated same-material PTDE-to-stock transition. The first tracer's slot hashing collided, exhausting 1024 lines in ~4.4s.

This revised tracer hashes the material slot into cache index entropy, includes `rowA/B` and `source_origin` in the transition condition, and samples every 500ms otherwise, bounded to 4096 records. Interpret `source_origin=1` as exact per-material selector producer; `2` as last native EnvSpec packer on same thread; `3` as last packer globally. Values `srcA` and `envdiffuseA` are read-only source signals. **If origin=2/3 and row shifts while owner/slot stay constant**, suspect cross-material last-source routing; do not force source substitution until PTDE operator/semantic proof. If origin=1 and row shifts, trace selector ownership, engine source pointer and LightBank row producer. These origin tags are observation only and never authorize material changes.
