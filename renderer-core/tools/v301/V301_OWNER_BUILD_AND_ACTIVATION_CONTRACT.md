# V301 owner-local single-addon activation (DIAGNOSTIC ONLY)

## Hard gates

1. Use the native Windows exporter tools produced by GitHub V300. Original six DSR PS DXBC are recovered locally from your Part02 archive, never published to a repository or CI artifact.
2. Run `renderer-core/tools/v300/run_owner_final_pipeline.ps1` with your Part02 ZIP, **new** result directory, V300 source tree, and downloaded V300 compiled tools. It verifies exact three Stable R17 and three Lerp R19 source SHAs, constructs V286 R5+R11+R7 for Lerp, then V297 post-LightScattering domain, validates original/candidate D3DReflect I/O and WARP CreatePixelShader for every RX, and writes `V300_OWNER_LOCAL_RECEIPT.json`. Any mismatch fails atomically.
3. Run `python renderer-core/tools/v301/assemble_owner_sidecar_diagnostic.py <completed_v300_owner_dir> <new_v301_diag_dir>`. It verifies all three V297 candidates and the V300 receipt, then produces **three owner-local DXBC sidecars and an approval header with their true SHA256 and lengths**. Do not invent/replace these hashes.
4. In a private local build checkout derived from R43, replace `renderer-core/include/dsrrl/runtime/hemenvlerp_v301_owner_approval.hpp` with the generated owner-approved file before compiling. Turn on `DSRRL_PMETAL_V301_OWNER_DXBC_DIAGNOSTIC` only together with certified V287 R19 route, Stable post-LS and explicit V289 diagnostic authorization. Keep V299 source row provenance optional. Produce one `.addon64`; put the three generated sidecars in `Dark Souls Remastered/DSR/DSRRL/EnvSpec/HemEnvLerp/v297_RX{33,34,35}.dxbc`.
5. The addon uses the *real* R19 shader as a SHA verified original/source-family trigger and, **only for RX33-35**, reads the corresponding owner-local V297 bytecode. It verifies 128-bit legacy DXBC checksum, full SHA256 and size against the compile-time manifest before passing it into `ID3D11Device::CreatePixelShader`.
6. Fail-open to original stock DSR on absent owner approval/header (default repository value is intentionally EMPTY), unknown material, receiver/hash mismatch, sidecar omission, checksum mismatch, or D3D11 shader create failure. The original `latest_hook_source` must remain exactly R43 (Renderer PROTECT Supabase revision 13040); Fog remains stock DSR. Do not modify DarkSoulsRemastered.exe on disk.

## Evidence status

CONSTRUCTION: source committed, native CI requires explicit confirmation. COMPATIBILITY: cannot be certified until final owner V286/V297 blobs and WARP acceptance. RUNTIME LIVENESS: OPEN. BRIDGE ACTIVATION: OPEN. PTDE PIXEL: OPEN. Offline operator/CB equivalence on synthetic paired data is not a substitute for in-game per-draw producer/resource mapping.

This is **not a live release**. A one-addon build with an empty SHA allowlist is a tested quarantine, not activation. Never claim a matching checksum from an unexecuted owner pipeline.
