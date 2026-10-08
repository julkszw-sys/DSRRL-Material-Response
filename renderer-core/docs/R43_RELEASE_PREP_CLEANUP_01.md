# R43 RC1 release-preparation cleanup 01

**Status:** DEVELOPMENT / CONSTRUCTION ONLY. Not an RC or public release. PTDE pixel behavior is OPEN and the prior R43 pixel regression has not been cleared.

## Base and exact scope

- Parent: build #227, commit `25c87f2718136a357f1e6c8b2a81ca236173f421`.
- No new shader replacement, asset, operator equations, resource authorization or PointLight draw logic.
- Fixed R40 / clustered Spc / clustered NoSpc PointLight physically excluded; stock DSR PointLight retained.
- Upper/Lower, HemDir3, Subsurface physically excluded; stock DSR paths retained.
- Material Response and exact P_Metal EnvSpec remain enabled with original receiver-first gates.
- P_Metal EnvSpec continues to fail open when the exact PTDE SpecRGB sidecar is absent, notably `wp_a_1535_s.dds` (Crest Shield).
- The live t0/t1/t2 verification retry from build #227 is preserved **unchanged**.
- MotionBlurTiles consumer-local camera-fallback fix and stock velocity producers remain untouched.

## Cleanup performed

`DSRRL_RELEASE_CLEANUP=ON` removes source- and receiver-heavy one-shot P_Metal diagnostics from the compiled addon:
- `[DSRRL PMETAL VALUE CUT]` full resource/LightBank diagnostic with its additional `PSGetShaderResources` companion probe.
- `[DSRRL PMETAL SPEC GATE]` first-reject native t1/MTD/sidecar probe and shadow recovery log, not the verification/recovery logic.
- `[DSRRL ENVSPEC APPLY]` per-stage once logging and associated atomic mask.

The original diagnostic source remains in GitHub behind compile-time guards; no deletion of reverse-engineering evidence.

The release-prep CMake guard rejects dangerous bisect switches, explicit telemetry defaults and SRV shadow experiments, and requires both physical-cut policies. Critical build identity and physical-cut startup markers remain enabled. The binary check explicitly rejects embedded diagnostic strings.

## Reproducible construction

GitHub Actions workflow `.github/workflows/r43-rc1-release-prep.yml` checks out an exact source commit, pins ReShade to `3645e3025d1d98a90e318278858931f034d5d1f6`, uses CMake/MSVC Release and archives the addon together with a SHA256 provenance manifest. All inputs are repository-source-controlled; no local binary edits.

## Required gates before real release

1. Native compile, link, source identity, physical-cut marker, diagnostic-strip verification.
2. In-game crash/restore and operator activation check with actual loader log.
3. Performance comparison against #227; no stale-shadow or source routing regression.
4. PTDE-vs-DSR pixel validation across stable and Lerp receivers, multiple maps/materials. Resolve known R43 pixel regression.
5. Exact PTDE equipment asset sidecars are optional per resource; missing sidecar always fail-open. `wp_a_1535_s.dds` remains unshipped until exact PTDE source is available.

Do not promote construction success to runtime or pixel PASS.
