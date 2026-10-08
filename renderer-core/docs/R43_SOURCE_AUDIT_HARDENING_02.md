# R43 RC1: source audit and hardening #02

## Scope and provenance
- Parent: source commit `24a20b1d20b716fce120c02eca9152f671c9c6f8`, addon build #228.
- Branch: `r43-code-audit-02`. Source and pinned GitHub Windows build are canonical. No `DarkSoulsRemastered.exe` write.
- This hardens the existing release-prep profile; **not an RC or release**. Construction / compatibility / runtime / operator activation / pixel behavior stay separate.
- PointLight physical cut, stock DSR PointLight, U/L-H3-Subsurface cut, P_Metal exact EnvSpec, Material Response, and MotionBlurTiles are unchanged by this patch.

## Code finding A — unadopted D3D11 COM class references (CONFIRMED from source)
Three `draw_state_transaction.cpp` entry points call `PSGetShader` with a local `ID3D11ClassInstance*` array. They previously exited when the shader was null or the returned count overflowed without transferring local class references into transaction state. The existing state cleanup only released adopted classes, leaving local `AddRef` results live. The fix explicitly releases **all non-null local class-instance pointers** before the existing fail-open exit. Normal capture/restore is byte-equivalent in intent. Rare failure path; do not claim an observed leak in a user run.

## Code finding B — DDS loader resource ceiling (CONFIRMED from source)
`material_resource_draw_runtime.cpp` read a complete file into a vector before inspecting the texture width/height or checking file size. A corrupt or oversized file could consume enormous CPU memory from the D3D init-view callback. The new 512 MiB ceiling runs **before the allocation**. Maximum admitted 16384x16384 BC7 with full mip chain is ~342 MiB, so this excludes files larger than the supported texture payload envelope plus normal headers. Excess data fail open as unsupported. All existing SHA256 attestations remain.

## Code finding C — sidecar path exception boundary (CONFIRMED from source)
`sidecar_path` allocates `std::filesystem::path` and may throw; three call sites previously constructed the path before entering `load_dds(... ) noexcept`. An exception could escape a ReShade resource-view callback. A `safe_load_sidecar(... ) noexcept` wrapper now wraps path construction and load, returning `unsupported` on exception. Original name, equipment allowlist, SHA, device/sampler/format checks unchanged.

## Performance improvement (source-level, not benchmarked)
`pmetal_env_source_runtime.cpp` has nine unconditional relaxed atomic increments whose values are only read for diagnostics: hook single/blend events and publish/consume counts. They now compile out only under `DSRRL_RELEASE_CLEANUP`; debug builds preserve prior counters. This reduces atomic cache-line traffic on hooks. Critically, all semantic versions, producer serials, cache generation and exact-key hit selection remain unchanged. **FPS/frametime improvement unknown** until measured against build #228.

## Automated checks
- `renderer-core/tools/audit_r43_release_cleanup_hardening.py`: source-level assertions on the three COM exits, pre-allocation file cap, all sidecar call sites, release-only diagnostic counters, exact resource path preservation, and physical-cut flags.
- Dedicated Windows MSVC CI: pinned ReShade API, `/W4 /WX`, `Release`, binary SHA, embedded source identity, stripped log strings, physical-cut marker and archive manifest.

## Residual risk register (OPEN)
1. Wider `on_init_resource_view` cache insertions may still allocate/throw in a callback after the safe path wrapper. Converting the whole cache update to no-throw transaction requires careful COM ownership semantics and tests.
2. Thread-local companion cache retains COM references until eviction or thread exit. Device teardown and multi-thread lifetime need runtime validation; do not eliminate the TLS fast path without profiling evidence.
3. `pmetal_env_source_runtime` exact producer cache and selector lifecycle are sensitive to R43 prior pixel regression; no semantic producer or cache-key changes in this hardening.
4. Crest Shield `wp_a_1535_s` exact PTDE companion unavailable; stock DSR fail-open remains.
5. Build #228 runtime and PTDE-vs-DSR pixel equivalence never validated; no equivalence claim can follow native CI.
6. Static source checks are not comprehensive dynamic memory, GPU state, or multi-thread testing. No perf benchmark performed.

## Validation progression
Native/marker/source static checks first. Then test ReShade load/unload, shader identity, COM resource reload, device reset, consecutive map transitions, stable+HemEnvLerp receiver activation, fail-open for missing texture, release-vs-#228 CPU frametimes with shader/model workload matched, and PTDE-visible pixels. Do not widen the bridge or modify stock DSR PointLight without separate evidence.
