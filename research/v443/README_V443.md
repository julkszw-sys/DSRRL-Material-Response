# DSRRL V443 — native actor / source packet read-only diagnostic (2026-11-06)

Parent integrated V439 runtime source: `1f6bc31c1b28755ed226d24de51717aba6ce8184`.
Frozen active development root V359-RC1: `6e852cdc9085dd175159e564e223e0cc8589c535`.
Original retail EXE SHA256: `a45aaa36dd2f6cc151670a639ea5547043cf38ea79ff4178b963c6ed71f98d7b`.

**Status:** V443 SOURCE / offline static preimages PASS. Win64 native build and gameplay runtime NOT TESTED; no claim about flicker fix, actor generation, or PTDE pixel behavior. This is a diagnostic, not a canonical LightBank bridge.

## Change

The existing protected Source1 + R43 fallback remain unchanged. V439 automatic capture + no F9 + crash-suspect V438 wrapper detours OFF remain unchanged. V443 introduces no new detour site or Shader/CB/RTV transaction. It samples only native packer hook calls with selector 1310/1311, uses unwind to recover emitter original caller (type5 R14 dp / type6 R15 dp), verifies original FrpgDrawParam + actor vtable, looks for exact default builder return and compares native dp A/B/beta to native draw packet A/B/beta. Alternate builder is recorded as partial rather than guessed.

`status=9` proves only a runtime snapshot of this default-builder frame/packet once the test runs; it does **not** certify actor lifetime generation or exact mapping to a physical screen pixel. No output record changes a render decision. Unexpected stack states stop observation, not rendering.

`v438::ACTOR_NATIVE` kind 15 schema:

- v00 packer kind (1 single, 2 blend); v01 selector A; v02 verdict; v03 emitter family;
- v04 native emitter return RVA; v05 builder return RVA; v06 entity; v07 embedded dp; v08 actor vtable;
- v09 typed model pointer; v10 model vtable; v11 packet pointer;
- v12 dp A, v13 dp B, v14 dp beta raw bits; v15 packet A, v16 packet B, v17 packet beta raw bits;
- v18 builder RDI, v19 builder RBP; v20 source A;
- v21 (single) native producer serial, or (blend) source B; v22 (blend) native producer serial.

Static safety/contract guards: exact original six E8 native emitter call sites, two default and one alternate builder call sites; R14/R15 save preimages; 11 in-memory mutation negatives. A runtime mismatch is captured with a non-success verdict and never patched to a desired row.

## Reproducible source integration, branch V439 ancestor

Copy the header into `renderer-core/include/dsrrl/runtime/` and the three `v443_*.py` tools into `renderer-core/tools/`. Then on Windows 2022 CI:

```powershell
python renderer-core/tools/v439_prepare_build.py
python renderer-core/tools/v439_name_binary.py
python renderer-core/tools/v439_marker_binary.py
python renderer-core/tools/v443_prepare_build.py
python renderer-core/tools/v443_name_binary.py
./renderer-core/tools/build_v438_one_shot.ps1 -ReShadeInclude ".../reshade/include"
```

The V443 preparation appends to V439's existing V438 overlay chain; `v443_actor_probe_overlay.py` edits source only AFTER all V439 patch stages. Build outputs one `out-v438-one-shot-diagnostic/DSRRL_V443_Packer_Actor_ReadOnly.addon64`. The original exe is never changed. The workflow provided in the ZIP pins ReShade `3645e3025d1d98a90e318278858931f034d5d1f6`.

The Windows CI job and its output are not represented as passed until an actual run. Due to previous V438 crash, V443 requires a fresh runtime/liveness test with crash markers; successful compilation alone is not enough.

Offline:

```bash
python verify_v443_native_join.py "DarkSoulsRemastered.exe" V443_STATIC_RESULTS.json
```

If gameplay CSV is available:

```bash
python analyze_v443_capture.py V438_*.csv V443_ANALYSIS.json
```

## Root-cause decision logic

A single native actor/packet tuple and issued candidate with same producer serial+model is stronger than V439's row-only observation, but *is not sufficient* until actor lifecycle/generation and physical FLVER/MTD/slot → exact issued RX34 receive a certificate. If native dp changes for one certified actor generation, trace the setter; if native dp remains stable but issued row differs, inspect Source1 ownership; if both match, inspect the operator-specific PTDE/DSR receiver-material-resource forward. No auto row freeze or global material-response adjustment is authorized by this diagnostic.