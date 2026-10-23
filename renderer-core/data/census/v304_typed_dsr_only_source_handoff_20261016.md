# V304 — Typed DSR-only donors as ACTIVE source records

Origin: `renderer-core/data/provenance/v300_lightbank_donors_ptde_dsr_only.tsv` already contains **1249** records: 1246 PTDE + three DSR-only (`a14/m14 row64`, `a14/s14 row64`, `a18/m18 row64`). V13's exact PTDE source C++ array and PTDE EnvDiffuse BIN stay byte-identical at **1246**; V304 adds the typed three through `v300::k_dsr_only` lookup. No false PTDE equivalence is asserted for those DSR-only records.

New source path `materialize_v304_typed_dsr_only_donors.py` (after V303):

* `read_exact_source()` returns exact native DSR EnvSpec row64 values after structural bank signature/layout/live count check, through the normal exact endpoint cache rather than returning an unknown row.
* `exact_envdiffuse_endpoint()` sources the matching exact DSR EnvDiffuse value instead of failing at PTDE BIN bound. Both A/B endpoints are handled independently.
* `pmetal_envspec_draw_runtime::prepare()` rejects the WHOLE PTDE P_Metal island before D3D11 changes if either endpoint is an exact DSR-only row64, retaining original complete native DSR operator via existing draw fail-open.
* The owner-protected `pmetal_env_source_runtime::latest()` and `latest_hook_source()` are unchanged, including their original R43 fallback call and producer lifetime.

Evidence caveat: source-level structural routing and Windows construction do not prove actual producer/selector/consumer activation in-game. A stale global fallback can still provide a different donor if both exact selector and current packer do not publish; this must be falsified in runtime row63→row64→row63 and A/B blend captures. Pixel target for DSR-only rows has no homologous PTDE operator; keep entire native DSR operator. 

Build via `renderer-core/tools/build_v304_typed_dsr_only_atomic_one_addon.ps1`; GitHub Actions `.github/workflows/v304-typed-dsr-only-atomic.yml` checks both typed source and native single-addon construction. Release blocked pending in-game provenance/activation/pixels. PROTECT rev13040 honored. No executable disk modification.
