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

## Composition rule: SpecRGB cannot pass pixel tests by itself

**Transport readiness** for equipment SpecRGB is narrow: exact owner+slot/MTD,
actual bound stock t1 logical identity, allowed PTDE Specular companion SRV,
certified t10 receiver, and stock t1 roughness/alpha preserved.

**PTDE pixel-behavior readiness** for HemEnv is stricter, but it is not an
extra prerequisite on independent resource-only operators:

```text
PTDE EnvSpec material pixel:
    receiver/MTD exact
    + PTDE SpecRGB t10 with stock t1 roughness
    + correctly mapped PTDE EnvSpec cubemap t12 (t14 if blended)
    + PTDE material response c100/c101 + verified private consumer
    + source endpoints, beta, and complete downstream composition
    -> pixel candidate, NOT pixel PASS until reference comparison
```

The entire HemEnv private shader must fail open to stock DSR if a consumed
component is unverified. Source=1 publication and Diffuse/Normal swaps are
independent. Neither a successful t10 bind nor a cubemap upload authorizes
a hybrid PTDE SpecRGB + stock DSR PBL/F0 renderer output.

The runtime now uses a single `material_chain_ready` condition on both
stable HemEnv and HemEnvLerp private shader transactions; the latter
separately requires the exact terminal SAT operator. PTDE EnvSpec resource
preparation remains before SpecRGB. The *t10 resource-only preflight*
instead requires only the actual equipment owner/t1 logical identity,
corresponding PTDE sidecar and certified SpecRGB consumer; it must not
be vetoed by the unrelated generic `full_material_response_ready` bit.
The caller owning the complete HemEnv private shader validates the combined
MR+EnvSpec+SpecRGB operator before committing any mutation.
The equipment-only SpecRGB PROTECT is unchanged.

## Disabled operator fast path

The generic resource router only resolves a bound t0, t1 or t2 companion when
that operator is enabled and its receiver family is supported. No stock SRV
identity/sidecar lookups are run for disabled classes. This is a performance
filter, **not** a source-authority veto; exact owner still applies to
equipment t10, whereas t0/t2 use actual stock SRV identity. The material
response/cubemap/pixel check stays on the private HemEnv shader.

## Exact zero-beta Lerp reduction

For a verified HemEnvLerp source and finite endpoints the PTDE linear
composition is `E=A+beta*(B-A)`. When `beta == 0.0f`, `E=A`
regardless of B. The runtime now aliases PTDE B cubemaps to A
(`t14=t12` for EnvSpec, `t13=t11` for EnvDiffuse), uses the same sampler
and exact A EnvDiffuse source for c84, and skips a separate B probe lookup.
The private Lerp shader still receives valid resources at both slots.
This is **exact zero only**, not an epsilon or a visual heuristic.
For any nonzero beta, require the original exact two-endpoint routing,
two valid PTDE cubemaps and separate c86/c84 signals. Full pixel
equivalence still requires runtime testing.

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

## V230 continuation: normalized B-bank source + exact RX34 witnesses (2026-10-24)

**Source-level correction:** Native `envspec_blend_hook_entry` receives its
original `source_a/source_b` pointers. With finite `beta >= 1.0` and
different selectors, `pmetal_selector_policy::select()` reduces the
expression to B-only, with normalized `endpoints.a=B`, `beta=0`.
Reading this normalized B selector from **native A bank** is incorrect.
The hook now reads from `source_b` and publishes that same selected bank
as its exact donor identity, while passing the original input pointers and
beta unchanged to the host packer. The protected `latest_hook_source`
TLS/global fallback is unchanged.

**Receiver-first runtime evidence, not a new patch authority:**

- `[DSRRL RX34 RESOURCE ID WITNESS]`: six bounded RX34/route345
  unresolved-sidecar snapshots, three stock slots each. Records native
  t0/t1/t2 SRV handles, DXGI view format/dimension, registry logical hash
  and independent sidecar status. `AMBIGUOUS_NATIVE_VIEW` means the
  existing quarantine gate remains in effect; `NO_NAME_SIDECAR_REGISTRY_ENTRY`
  means the callback registry has no authoritative identity *for that view*,
  but does not distinguish a missing name from a missing matching sidecar.
  `TRACKED_NO_CLASS_SIDECAR` means the registry has an entry but the
  requested class replacement is unavailable. Unrequested slots are marked
  `requested=0` and cannot be considered failed operators.
- `[DSRRL RX34 T11 DOMAIN WITNESS]`: eight bounded exact P_Metal
  RX34 post-source reads of actual PS slots t11, s11, b12 and the native
  t11 resource descriptor (view/texture format, width, height, mip count,
  array count and misc flags), plus exact source EnvDiffuse readiness
  and beta. The witness reads state with `PSGet*`; it does **not** write
  `PSSet*`, bind replacement SRVs, create a shader or establish a PTDE
  probe alias. A non-NULL t11 is not proof of PTDE asset homology.
  The witness does not run if an earlier material/source gate rejects. On a deferred context, native PSGet state may differ from the ReShade descriptor shadow; a NULL native t11 alone must not be interpreted as absence of the shader-visible t11.

The two witnesses intentionally answer different questions: `t0/t1/t2`
companion admission is not `t11` EnvDiffuse identity, and a source donor
is not a pixel consumer. In the next owner-run matched-scene log, join both
markers with the exact RX34 route/material/slot and the existing source
stage counters. Preserve stock DSR on unresolved identity; **do not**
turn either diagnostic into a new bypass.

**Status:** source-code change committed, CI native build to be checked on
the new HEAD; in-game RX34 hit, exact PTDE t11/c86 consumer activation,
state restoration and PTDE pixel improvement are still OPEN. The active
V359-RC1 release is not replaced by this draft.
