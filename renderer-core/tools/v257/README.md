# V257 — PTDE equipment HemEnvLerp clean operator

**State: STAGE-1 CONSTRUCTION.** This is new code, not a patch to R19. It is
not bound to a D3D draw and is not a new playable addon. The retained V255
binary is *not* relabeled as V257. No PTDE pixel or runtime equivalence claim.

## Scope

Only equipment/armor material slots with physically present and verified DSRRL
sidecars, an exact FLVER + MTD + shader + texture owner, and a verified
receiver-specific PTDE consumer. Unknown bodies, world geometry, nonhomologous
materials, absent sidecars, unsupported shaders or incomplete atmospheric inputs
must **fail open** to their untouched stock DSR path.

The independent C++17 header uses full SHA-256 asset identities and separate
receiver and slot checks; it deliberately does not admit a candidate from an
MTD basename, shader family name, probe ordinal, or DSRRL filename alone.

## Original PTDE operators ported into standalone source

* PTDE ps_3_0 DifSpcBmp HemEnvLerp **0511–0542**: sample EnvDiffuse B/A
  as raw filtered RGBA; RGB divided by filtered alpha, multiply B by c84.rgb,
  A by c86.rgb, combine with independent c84.w.
* **0611–0642**: EnvSpec cubemaps B=s14 and A=s12, filtered RGBA decode,
  B*c85.rgb and A*c87.rgb, interpolate using independent c85.w.
* **0651–0679**: PTDE SpecTex RGB * c101 * vertex RGB multiplied by the
  EnvSpec result; diffuse material factor c100 * vertex RGB *
  (DiffTex RGB+c156) multiplied by EnvDiffuse + the previously evaluated
  Upper/Lower signal; add both. This cut does not read SpecTex alpha.
* **0782–0794**: c110.w late interpolation between true post-Fog and true
  post-LightScattering RGB, then c135.x/c135.y. **Not** the EnvSpec beta.

Reference evidence: user V253 technical handoff,
`PTDE_DifSpcBmp_HemEnvLerp_annotated.txt`, SHA-256
`3c3ce5c7e344efc0c573d9e501090781104bd87cf1c17c03093c893e378c0872`;
Supabase canonical rev1177/3818/12842 and rev8647. This is a family-local
reference kernel. Do not extend to different Phn, MTD, or SSS consumer families
without a separate exact census.

## Explicitly NOT ported yet

This stage does not contain actual GPU cube sampling/LOD, normals/reflection
generation, full DXBC RDEF/create-time shader replacement, exact PTDE
Fog/LightScattering producers and ordering, draw-scoped D3D11 CB routing, nor
full material/FLVER/sidecar on-disk authority binding. SAT/gamma changes are
not inferred. The material-specific body/Subsurface path is not activated.

The terminal function accepts already verified PTDE atmosphere stage outputs.
A caller may **not** supply stock DSR outputs and claim PTDE-equivalence.

## Sidecar inventory

`python renderer-core/tools/v257/inventory_dsrll_sidecars.py --root
<DSRRL directory> --out <manifest.json>` reads DDS and PackedGI file bytes,
validates DDS header and hashes all discovered sidecars. It emits
UNVERIFIED_CANDIDATE records, *not* a render whitelist. The separate
sidecar routing proof requires full physical SHA256, exact logical texture
identity, verified FLVER SHA256, MTD SHA256, shader SHA256 and slot.

## Verification

`g++ -std=c++17 -Wall -Wextra -Werror -pedantic -O2 -Irenderer-core/tools/v257
renderer-core/tools/v257/test_ptde_equipment_hemenvlerp_core.cpp -o /tmp/v257 && /tmp/v257`

Windows MSVC and Linux C++17 tests compile and run the exact core with synthetic
samples, independent channel weights, material combination, c110/c135 terminal,
and fail-open controls. They do **not** provide in-game validation.

**No changes to latest_hook_source**, active R43, any renderer shaders, DSR exe
on disk, existing V255 addon, or unrelated DSR effects.
