# V412 — Bounded per-context native producer history (receiver RX34)

**Goal:** Eliminate false Source1 producer-to-draw mismatches introduced by the *diagnostic* V411 latest-only owner journal when multiple native packer stamps for the same model owner exist before the D3D indexed draw. Preserve the host DSR rendering path and all protected source fallback semantics.

## Root cause established in V411 source

`v411_owner_sequence_journal.hpp` used a single `witness value` per `owner`; each later `observe(owner, newer_native_sequence)` replaced the previous record. Subsequent `indexed_issued` RX34 observations can correspond to older **prepared** records even if the current producer cache has already advanced, so a latest-only diagnostic cannot distinguish a valid in-flight record from a provenance mismatch. This is a **source-code-proven diagnostic limitation**, not a demonstrated RX34 flicker cause.

## V412 construction

- Maintain eight bounded snapshots per exact owner, organized in 64 sharded locks × 64 owner slots. Lookup uses **owner + native_producer_sequence + selector_epoch**. The key does not collapse to LightBank row, FLVER hash, or global selector epoch.
- Every snapshot keeps full native wrapper, LightBank manager, source A/B pointers, selectors, row A/B, beta bits, vtable and independent manager field witness. Duplicate stamps with identical provenance and source payload **do not** consume an additional slot.
- A match from an earlier retained slot sets `superseded_by_newer=1`. It is still only a journal match. The current Source1 cache might have advanced; V412's log parser calls that `historical_stamp_cache_advanced_actor_open` rather than incorrectly claiming a corrupted source.
- Repeated same native sequence+epoch with conflicting context tuple or endpoints returns `ambiguous_context_identity`, not success. If wrapper/manager changes while a prior slot is retained, historical matches lacking independently verified current wrapper/manager also remain ambiguous. This explicitly preserves the **ABA/lifetime frontier**.
- A query not found after earlier records have been overwritten returns `history_evicted`. No speculation that the corresponding native packer did not exist; no speculative eviction of another owner.
- Heap-backed 4096-owner journal, instead of multimegabyte thread stack; nothrow allocation failure yields a missing/capacity diagnostic, **never** a change to source selection or the DSR host.
- Original numeric `status` values 0..5 remain unchanged; new states appended 6=`history_evicted`, 7=`ambiguous_context_identity`. RX34 logs `historical_snapshot`, `history_evicted`, `stamp_candidates`, `history_overwrites`.
- Existing protected R43 `latest_hook_source()` and Source1/Source2 eligibility, LightBank decoding, RGB, material matching, shader state, GPU payload and executable on disk are untouched.

## Reproducibility

Branch: `agent/v412-context-history-no-stale-joins` in `julkszw-sys/DSRRL-Material-Response`.

Source:
- `renderer-core/include/dsrrl/runtime/v411_owner_sequence_journal.hpp` (changed data structure; ABI in diagnostic-only header)
- `renderer-core/include/dsrrl/runtime/v411_rx34_owner_draw_trace.hpp` (bounded history observability)
- `renderer-core/tools/analyze_v411_owner_draw_log.py` (honest classification of superseded snapshot)
- `renderer-core/tests/v412_context_history_tests.cpp`, `renderer-core/tools/test_v412_owner_history_log.py`, `renderer-core/tools/test_v412_owner_history_contract.py`.
- `renderer-core/CMakeLists.txt` registers native test; `.github/workflows/v412-context-history.yml` builds one integrated Windows addon and runs native/contract tests.

Local tests: C++17 GCC `-Wall -Wextra -Wpedantic -Werror` 8 workers × 1000 iterations PASS. Previous V411 native tests PASS. Clang AddressSanitizer + UndefinedBehaviorSanitizer PASS. Parser synthetic tests PASS. GitHub source regression check 13/13 PASS. **No Windows build or live RX34 runtime has yet been verified**, including any GitHub Actions run. The new source does not constitute a compiled addon artifact.

## Remaining semantic cut and falsification

A V412 log can demonstrate a matching **retained context snapshot** even when Source1 cache has moved to a newer producer before draw. It still does **not** prove the native model's actual constructor/destructor lifetime generation, command-list order, shader CB consumer, or PTDE pixel equivalence. An owner pointer can be reused after native destruction; vtable+manager field checks alone are not a lifetime certificate.

Next experiment: build a single `.addon64`, record V408/V411/V412 for RX34 while inducing Source2 row30 ↔ Source1 row25, then join actual native wrapper's call and context lifetime with the issued draw and bound GPU CB. If no reliable identity is available at a specific receiver, the lighting operator must fail-open to stock DSR.

**Status:** CONSTRUCTION LOCAL PASS; COMPATIBILITY WINDOWS OPEN; RUNTIME LIVENESS OPEN; BRIDGE ACTIVATION OPEN; PIXEL BEHAVIOR OPEN.
