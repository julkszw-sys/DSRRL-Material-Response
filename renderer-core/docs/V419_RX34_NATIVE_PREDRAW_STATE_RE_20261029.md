# V419 — RX34 native pre-draw state cut

2026-10-29. Renderer Edition, diagnostic-only descendant of V418 pinned source 711af6e03fe456e751b085e26d3eae8bf1a51257. Frozen production V359-RC1 and game executable on disk unchanged.

## Confirmed source timing

V418 logs its joined record at indexed_issued, AFTER dispatch_island_draw_indexed_batch returned. Native D3D11 transaction code begins by capturing and applying state, then invokes DrawIndexed/DrawIndexedInstanced and restores native D3D11 state. Thus reading the native PS/CB/RTVs from an indexed_issued callback would read restored host state, not the state that the island passed to the native draw.

V419 moves the read-only capture to draw_state_transaction_runtime::replay_draw_indexed at the precise boundary AFTER begin() applied native state and BEFORE ctx->DrawIndexed. This is the narrowest available native callsite for effective D3D11 bind-state observation without introducing a detour or draw replay.

## Exact activation

Only exact STABLE RX34 Source1, verified V413 FrpgDrawContext wrapper/manager, V411 one matching journal record for native producer seq + epoch, verified V415 model, V416 reciprocal FgModel owner, readable/allocated V417 generational handle, matching source row and no journal supersession can arm a V419 capture. Additional guard matches native D3D11 context pointer. Any failure leaves capture inert; protected R43 fallback and stock DSR operator policies are unchanged.

Sampling is bounded: first 256 exact opportunities and every 8192nd thereafter. No blanket per-draw API calls in the normal game. The source material/FLVER and raw MTD SHA256 are copied to TLS before draw.

## Exact native state sampled

At pre-draw, read-only native Get methods obtain current PS binding, PS b12 buffer pointer and ByteWidth, RTV 0-3 and DSV pointers, RTV0 format/dimension/texture size/sample count, DSV view format, index buffer pointer/index format/index offset, first viewport dimensions and draw geometry arguments. The capture also tags immediate/deferred D3D11 context and compares actual PS/b12 to exact prepared mutation expectations when available. All temporary COM references are released; no Set, Map, UpdateSubresource, extra Draw or replay is introduced.

Diagnostic marker: [DSRRL V419 RX34 PRE-DRAW NATIVE STATE]

## Boundaries of certainty

This certifies state binding at native DrawIndexed call, not CB byte contents or shader-equation C86 consumer. D3D11.1 CB offset/first constant is not independently attested. On deferred contexts it certifies only command recording, not GPU command completion or presentation. RTV/DSV and geometry are pass evidence, not proof that the observed draw contributed to the visible armor pixel. PTDE pixel behavior remains OPEN until runtime screenshot/trace validation.

The offline script analyze_v419_rx34_native_predraw_log.py compares row30 and row31 ONLY for exact context, model, FgModel, generational registry, material slot, FLVER and MTD SHA. Reports PS, b12, RTVs, DSV, viewport, geometry and confidence limitations. Missing sampled events are UNKNOWN.

### Reproduction

Windows one-addon build via renderer-core/tools/build_v419_rx34_native_predraw_diagnostic.ps1 with pinned ReShade SDK include path. Source contracts via renderer-core/tools/test_v419_native_predraw_source_contract.py; native V418 journal tests remain required.

Status: static source-chain RE CONFIRMED. Windows CI and addon build separately verified. Actual V419 runtime liveness, bridge activation and PTDE pixel remain OPEN.
