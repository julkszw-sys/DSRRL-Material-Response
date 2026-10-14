# V317 / post-V272 integration acceptance matrix

**Target:** V272 equipment SpecRGB runtime fidelity **plus** each later justified independent
operator correction, with modern owner V286 HemEnvLerp. No blanket rollback, and
no promotion based solely on native CI. This draft is BLOCKED for release.

| Lineage | Evidence / effect | Required treatment | Status in current V317 |
|---|---|---|---|
| V272 V203/V206/V214/V216, full equipment SpecRGB | Owner runtime positive for equipment sidecars, excluding Lerp; exact equipment MTD/SRV routes and 1295 whitelist | Preserve exact 36 equipment EnvSpec routes, full sidecar authorization and matching material consumers | Source unchanged, runtime still OPEN |
| R43 producer source and per-key freshness cache | Source exists largely in both V272 and R43 descendants, but builds use different compile-time flags | Diff effective compiled behavior, not only filenames; preserve protected latest_hook_source | OPEN |
| V273 interior beta c84/c85 producer mapping | Exact PTDE x86 CPU branch finding; V272 beta is DSR sourced/unobserved | Integrate genuine source/consumer mapping only after end-to-end cut, otherwise log missing producer without inventing values | OPEN, NOT ported |
| V275 synthetic zero LightBank descriptor guard | Defensive invalid-source suppression; owner rejected claimed visual fix without evidence | Preserve only with exact source proof; not a required pixel-fix claim | REVIEW, NOT ported |
| V281 / V286 R5-R11-R7 RX33–35 Lerp shader | Exact original donor reconstruction, normal-basis operator and owner 3-RX checksum/SHA | Bind exact V286 output on RX33–35 and never allow obsolete active R19 or dormant V270 to win; avoid touching other equipment SpecRGB | Source overlay STAGED; Windows build NOT PASS |
| V287 exact-only admission | Strict exact shader identity/receiver SHA and fail-open | Keep exact per RX/hash and stock DSR fallback when no authorized sidecar | Partly STAGED |
| V289–V297 Fog/LightScattering postdomain and NOP trials | Operator investigation; user reported regressions and explicitly requested removal of custom Fog/LS changes | Preserve stock DSR Fog/LS, not the experimental 21-NOP patch; retain research data only | Excluded by current V286 choice |
| V298 beta continuity / scattering affine model | Static / numeric evidence, not deployed corrected producer | Keep as future validation oracle, never count as runtime fix | NOT A RUNTIME FEATURE |
| V299 donor row provenance and PROTECT | Three DSR-only native rows vs 1246 PTDE donor rows; avoids unjustified remapping | Port exact source classification if applicable, without modifying protected latest_hook_source | OPEN, NOT ported |
| V300/V301/V312/V313/V315 packaging | Real owner DXBC SHA and path, single-addon integration; 313/315 owner regressions | Correct game-root `DSRRL/EnvSpec/HemEnvLerp`, one addon, actual owner SHA and fail-open; WARP not required by owner | Exact loader source STAGED; no compiled V317 |
| V315 source-construction regression | V315 CMake dropped V272 equipment V203/V214/V216 options while generic SpecRGB resource code remained | Never transplant V315 CMake wholesale; require V272 route parity at build and runtime | Protected V272 source tree |

## Release gates

1. Source provenance: V272 exact baseline + audited per-operator later patches; no accidental restore of obsolete Lerp.
2. Construction: one native `.addon64`, verified actual owner RX33–35 V286 SHA. No WARP requirement.
3. Equipment regression: exact compiled configuration and runtime coverage for nonmetal/leather/body/metal material-slot sidecars, not only P_Metal.
4. Lerp activation: exact receiver/owner, SRV/CB ABI, per-RX sidecar hit, actual D3D11 replacement with stock fail-open and no old R19/V270 shader.
5. Visual validation: PTDE operator behavior / pixels, multiple materials/maps/receivers, separate from build or runtime success.

**Do not mark complete** based on the present V317 branch. It currently has a
Windows CI failure at the V272 script-insertion anchor, before native compilation.
