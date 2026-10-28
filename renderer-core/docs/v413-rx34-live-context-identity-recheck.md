# V413 — RX34 Live FrpgDrawContext Identity Recheck

**Scope:** Only diagnostic tracking of CPU native LightBank producer ↔ context ↔ RX34 `indexed_issued`. Original `DarkSoulsRemastered.exe` on disk is never modified. No Source1/Source2 eligibility, C86, RGB, CB, shader, material or asset replacement changed.

## Source-level bug identified
V411/V412 `indexed_rx34` called `journal.lookup(owner, native_seq, selector_epoch)` **without** passing its optional `observed_wrapper` and `observed_manager`. Therefore the `status::context_tuple_changed` check for live wrapper/manager could not run at issued draw. A pointer-only owner key cannot prove identity after context memory reuse.

## Implemented correction
1. New header `v413_live_owner_probe.hpp` reads native current owner first-pointer/vtable, `owner+0x2318` wrapper, `owner+0x2320` manager and `wrapper+0x40` manager with `ReadProcessMemory(GetCurrentProcess())` (failure without direct unsafe dereference), only on sampled RX34 first-source/transition events. It reports `context_unreadable` or `native_manager_disagreement` instead of claiming success. These are *observations*, not receiver gate changes or lifetime certificates.
2. `v411_rx34_owner_draw_trace.hpp` now passes the live wrapper and manager to `lookup()` and compares observed vtable with recorded vtable if the stamp vtable was readable; retains exact FLVER SHA/MTD SHA, native producer sequence, Source1 cache readback, and safeguarded Source2.
3. V412's 8-snapshot journal now retains a **monotonic observed context-run marker** for each owner. A retained older source under a **different wrapper/manager/vtable run** is `ambiguous_context_identity` even if owner, native sequence and epoch happen to coincide. A superseded source within the **same observed run** may match; an earlier sticky `had_pointer_reassignment` alone no longer overrejects new in-flight snapshots. This is a *run of observed field values*, not the engine's actual constructor generation.
4. Two diagnostic-only status codes appended while retaining enum values 0..7: 8 `context_unreadable`; 9 `native_manager_disagreement`. RX34 log includes `matched_run`, `latest_run`, `live_readable`, `live_manager_agrees`, `live_wrapper`, `live_manager`, `live_vtable`.
5. `test_v411_owner_sequence_contract.py` updated to test the current V412/V413 journal implementation rather than assert a deleted V411 latest-only storage expression. This fixes first V413 GitHub CI failure *without dropping requirements* for native sequence, selector epoch, exact Source1 packer witness or Source2 fallback.

## Test evidence
- Local Linux native GCC C++17 -O2 -Wall -Wextra -Wpedantic -Werror: V412 regression PASS, V413 context-run test PASS, 8 workers × 1000 events.
- Local Clang ASan+UBSan V413 context-run test PASS.
- GitHub Actions V413 `model-source-contract` on run 40674802025 PASSED; Linux source contracts, offline join classifier, compiler and native tests. Windows x64 `native-single-addon` job tracked separately, do not infer a PASS from Linux success.
- Windows native test `v413_live_owner_probe_windows_tests.cpp` creates readable context/wrapper, checks manager agreement/disagreement and verifies a released native context is reported unreadable. This test must pass on actual Windows CI before compatibility is considered verified.

## Remaining proof boundary
Even a matched live wrapper+manager/vtable+producer snapshot cannot certify the engine's **actual constructor/destructor lifetime**, because an allocator may reuse a context address with matching field values. It also does not certify async GPU command-list ordering or the final PTDE-visible pixel. The exact native ctor callsite RVA `0x1EC9CD` and deleting destructor `0x1EC480` remain possible independently validated observer boundaries; they are NOT hooked in V413. The old 14-byte copying trampoline is NOT used here.

**PROTECT:** R43 `latest_hook_source()` preserved, row25 and zero C86 are never rejected, unrelated receiver families stock DSR. Construction PASS. Windows addon build, runtime liveness, bridge activation and pixel behavior must each be assessed independently.


## Diagnostic build correction (V413 CI)

First CI: Linux source contracts PASS; Windows native compiler and linker **produced the addon**, but `build_v230_production.ps1` rejected it with `Missing binary marker: [DSRRL V408 CONTEXT SELECTOR CENSUS]`. Root cause is the existing **release-only** `materialize_v230_release_cleanup.py`: it forces effect telemetry disabled and suppresses INFO observers in `pmetal_env_source_runtime.cpp`. This is normal for shipping V230 but incompatible with V408/V411/V413 diagnostics. The compiler output is not evidence that the required observer was actually present.

The dedicated `build_v413_selector_diagnostic.ps1` generates a build without editing the frozen V359-RC1 or main production script. It uses separate `build-v413-selector-diagnostic` / `out-v413-selector-diagnostic` directories, an individually named `DSRRL_V413_Selector_Diagnostic.addon64`, and enables the existing CMake `DSRRL_EFFECT_TELEMETRY_DEFAULT_ON_ONLY=ON` option. `materialize_v413_selector_diagnostic.py` applies **only** the existing V296 nonhomologous cross-operator veto correction; it intentionally leaves V408–V413 read-only INFO observers intact instead of running the V230 log removal. `test_v413_diagnostic_build_contract.py` checks that native logs and the protected Source2 fallback survive.

Windows one-addon workflow is `.github/workflows/v413-rx34-live-owner.yml`; its diagnostic build must pass before any binary should be called testable. This diagnostic variant is **not a production release** and logging overhead must be benchmarked with the effect-enabled trace.
