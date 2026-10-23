# DSRRL V350 Scope Shell GPU Resident RC1

Status: release candidate for owner runtime testing; not PTDE pixel PASS.

## Immutable operator lineage
- Frozen V350 PTDE material, HEMEnvLerp RGB swizzle and PointLight operators and receiver gates retained.
- Owner-good Scope Shell PR446 source 00d4c2246b4471fa30fd907bb642a600a7833422 remains unchanged.
- GPU residency prototype PR447 source 1cfee5917687a4b8b321745faa974c38513eb20f.
- RC branch agent/v350-scope-shell-gpu-resident-rc1-20261017.

## Install
Deploy ONLY DSRRL_V350_SCOPE_SHELL_GPU_RESIDENT_RC1.addon64 alongside the existing PTDE sidecar directories. Do not load multiple DSRRL addon versions together. A residency TXT is NOT required: there is no DSRRL_SIDECAR_RESIDENCY.txt reader and prewarm is enabled by default.
Previous QPC diagnostic mode TXT files are not needed for this RC (Scope Shell defaults to mode 0).

## CPU/GPU transport
- init_device queries IDXGIAdapter3 local GPU memory budget; if unavailable, prewarm fails open.
- Only verified allowlisted Specular/Diffuse/Normals DDS are preloaded under half of currently free DXGI memory-budget headroom, preserving the remainder for native DSR streaming.
- All newly materialized cold sidecars obey the same budget. Sidecar SRVs are cached across stock SRV lifetimes, released on device teardown.
- SHA/format/FLVER/MTD/receiver/resource identity and shader family gates remain unchanged; merely existing on disk never authorizes a PTDE operator.
- EnvSpec PackedGI is a separate resource bridge. No changes to DarkSoulsRemastered.exe on disk.
- Budget uses sidecar DDS disk bytes as a GPU allocation proxy; not a residency/page-fault guarantee. Startup can become longer.

## Verify before release
ReShade logs: [DSRRL GPU RESIDENT RC1] default=ON config_file=NONE and [DSRRL GPU RESIDENT] mode=1 early_gpu_preload=N ... .
Confirm N greater than zero and compare worst FPS/frametimes at a repeatable streaming transition versus owner-good Scope Shell.
Build/construction PASS does NOT imply runtime activation, performance improvement or PTDE pixel equivalence.

Rollback: replace only the addon with the previous known-good V350 Scope Shell; retain unchanged sidecar folders and original DSR executable.