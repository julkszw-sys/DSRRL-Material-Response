# Install v2.0.2

Requires 64-bit Dark Souls Remastered and ReShade 6.8.x (API 20).

1. Back up and remove older DSRRL addon variants from the game directory.
2. Install only `DSRRL_Restored_Lighting_v2.0.2.addon64` next to `DarkSoulsRemastered.exe`.
3. Retain your existing exact PTDE sidecar resources; they are not distributed with this binary.
4. The ReShade log should include `version=2.0.2`, `epochs=KEY_BUCKET_256`, and `mode=4WAY_64SETS_256TOTAL_SYNC_ON`.
5. Verify SHA256 against `SHA256SUMS.txt`.

The QPC/hitch profiler is compiled out in the release build. The `.addon64` suffix is correct for 64-bit ReShade.
