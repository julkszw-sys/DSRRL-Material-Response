# V303 — Protected R43 producer fallback / row64 census (2026-10-16)

Branch `agent/v303-protected-r43-donor-census-20261016`; parent `567e2f436792038d51781d488c805c755436a201`.

## Construction change

The V300 materializer no longer rewrites `pmetal_env_source_runtime::latest()`. It preserves the original R43 TLS and synchronized global `latest_hook_source(out)` call with its existing lifetime. This satisfies the source-level requirement of owner PROTECT `renderer.pmetal_env_source.latest_hook_source` rev13040, but does **not** fix R43's documented stale cross-material snapshot risk.

The separately verified three DSR-only row64 identities remain independently classified using exact bank signature, layout signature, live count and PTDE donor count; the source logger says `live_stock_routing=UNVERIFIED`, not `action=stock_dsr_operator`.

## Unresolved runtime

The exact producer/consumer join from row64 selector to material/receiver draw has not been verified. After DSR-only decode failure, the protected R43 fallback may still return a stale PTDE snapshot. Thus this build does not demonstrate stock DSR row64 or prevent all PTDE leakage. No claim of complete 1295 atomic sidecar routing, in-game activation or PTDE pixel improvement is permitted.

Before release, independently authenticate source-keyed row63→row64→row63 and blended A/B transitions, verify exact material owner and receiver, and add consumer-local draw fail-open only where justified, without deleting/modifying the protected R43 producer path. Unknown routes must stay stock DSR, not guessed PTDE donors. This requires new routing evidence and scoped review.

## Reproducibility

Compile once using `renderer-core/tools/build_v303_protected_r43_donor_census_one_addon.ps1 -ReShadeInclude <pinned ReShade/include>`. CI `.github/workflows/v303-protected-r43-donor-census.yml` runs C++17 donor classifier, Python materializers/source contract, Windows x64 native CMake and creates exactly one `.addon64`. Source commit and build flavor embedded; /EHsc retained. Construction/compatibility pending CI, runtime/pixel OPEN, release NOT APPROVED.
