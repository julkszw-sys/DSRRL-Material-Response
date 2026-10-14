# V312 owner-private DXBC activation gate

V311R verified 15 owner DXBC offline. V312 separates **prepare** (default deny, compile-time `approved.size()==0`) from **authorize**, which requires a complete SHA-bound Windows WARP report for the exact 15 owner files before creating a 3-entry V301 diagnostic approval header.

- Never upload owner DXBC to public GitHub or CI.
- Exact RX33/34/35 sidecars remain in `DSR/DSRRL/EnvSpec/HemEnvLerp`.
- Copy the approval header into an isolated private build source tree; never mutate immutable R43 RC1.
- A WARP result is shader creation compatibility only, not in-game activation or PTDE pixels.
- Preserve stock Fog and `latest_hook_source`.
- Source reference/owner-private implementation: V312 private admission bundle.
