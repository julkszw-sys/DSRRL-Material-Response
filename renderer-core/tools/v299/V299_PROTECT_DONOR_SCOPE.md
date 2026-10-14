# V299: scoped donor provenance / LightBank PROTECT

Owner requires retaining `latest_hook_source`. Supabase PROTECT `renderer.pmetal_env_source.latest_hook_source`, revision 13040. The V299 descendant restores R43 exact source for `latest_hook_source`, its caller `pmetal_env_source_runtime::latest`, and material TLS producer. Source-identity regression tests enforce this.

R43 donor layout: 20 banks, 1249 native DSR rows, 1246 donor rows. Three DSR-only rows: m14/64, s14/64, m18/64. V299 classifies those IDs separately; they cannot resolve to another PTDE donor. EnvSpec and EnvDiffuse use this common donor source.

The protected global/TLS producer path remains available. Therefore an unmatched donor does not independently prove the final draw uses stock DSR; exact runtime source-to-consumer correspondence is still OPEN. Do not claim runtime or pixel PASS. R43 RC1 stays unchanged.

LightScattering continuation: V289/V298 reference only, Stable/Lerp must be paired and Fog remains stock DSR. No production activation before owner-final V286/R19 and pixel verification.
