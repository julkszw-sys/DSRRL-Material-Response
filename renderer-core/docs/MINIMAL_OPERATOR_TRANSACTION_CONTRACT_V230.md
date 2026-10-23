# Minimal transaction contracts — V230 resource-gate refactor

Scope: independent draw/resource transactions derived from the owner-verified
v2.3.0 pre-release source `232ac131`. PTDE behavior remains the target,
stock DSR the fail-open fallback. This is NOT authorization to bypass an
unidentified shader consumer or rewrite unrelated operators.

## The operational rule

Each transaction requires **only its own operands and actual consumer**.
Cross-operator completion/sidecar whitelists are neither implicit inputs nor
valid vetoes. Resource identity is established once, at live stock SRV
registration; the draw looks up the exact registered SRV and matching sidecar.
D3D11 draw-specific mutation must still restore the relevant original state.

- Diffuse: `supported Bmp RX + live stock t0 + same-name valid DDS SRV`
  -> bind t0 for the draw, preserve other resources. No t1, t2, FLVER,
  c100, EnvSpec or PointLight gate. Separate c100/MR remains independently
  authorized when its consumer is used.
- Normal: `supported Bmp RX + live stock t2 + same-name valid DDS SRV`
  -> bind t2, preserve stock s2. No Diffuse/Specular tuple.
- SpecRGB: `actual equipment material + exact specular t1 identity +
  exact PTDE SpecRGB sidecar + certified shader t10 consumer`. Preserve
  native t1/roughness unless the operator-local shader is independently
  authorized. A valid Diffuse/Normal sidecar cannot authorize SpecRGB.
- Cubemap / EnvSpec resource: `actual bound probe identity + compatible
  cubemap asset/view + correct shader resource slot & sampler`. Stable
  receiver needs A only; HemEnvLerp needs A+B if both are consumed. Never
  require unrelated Diffuse/Normal/SpecRGB for a resource-only cube bind.
- PTDE EnvDiffuse operator (not yet active in this patch):
  `exact HemEnv/EnvLerp consumer + current t11 (and t13 when blended) +
  PTDE probe/legacy identity + source c86 (and c84/beta if blended) +
  atomic shader/CB/SRV/sampler mutation with full restore`.
  It does **not** inherit t12/t14 EnvSpec, t10 SpecRGB, PBL F0, PointLight,
  or full Material Response prerequisites.

## Source=1 diagnosis (OPEN)

The canonical runtime log `201838` proves a source_kind=1 RX34 EnvSpec
transaction can complete, but its EnvDiffuse PTDE t11 is NULL even while c86
exists. The EnvSpec-only resource path never prepares the independent t11.
Therefore removing authorization branches alone cannot realize EnvDiffuse;
it needs its own t11/c86 shader/CB consumer. The experimental V369 standalone
branch was pixel-rejected because it retained DSR cb0 factors instead of the
actual PTDE c86. V370/V371 experimental work remains unmerged and requires
source lineage and runtime review. Never claim Source1 PTDE EnvDiffuse fixed
because an unrelated resource transaction built.

## Verification status

The C++ branch changes live resource registration, t0/t2 route decisions,
ownerless resource draw fallback, EnvSpec atomic-failure fallback and fixed
retail-DDS SHA attestation. Compilation is independently tracked in CI;
in-game draw activation and PTDE pixel remain OPEN until evidenced. The
protected P_Metal latest_hook_source fallback, PointLight, DoF and DSR EXE
remain untouched.