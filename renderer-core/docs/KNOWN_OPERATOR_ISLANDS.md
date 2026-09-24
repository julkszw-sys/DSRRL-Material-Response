# Renderer Core v1 — complete known-operator island catalog

This phase registers every currently known canonical renderer operator as an independent
island contract. Registration is not runtime activation.

## Canonical surface/light/post operators

The source-of-truth set is snapshotted from `dsrrl.shader_math_operators`.
Architecture-only `architecture.host_cut` remains a core contract rather than a feature
toggle. Every other row is mapped to an `operator_id`.

Current shader/operator islands include:

- Upper/Lower;
- HemDir3 D1/D2/D3;
- Diffuse material-domain linearization;
- EnvDiffuse;
- exact legacy EnvSpec;
- certified no-Spc EnvSpec deletion;
- historical P_Metal fixed-LOD EnvSpec diagnostic;
- full PointLight;
- PntS attenuation;
- local legacy PointLight specular;
- terminal RGB SAT;
- terminal RGBA SAT diagnostic;
- fixed-family post-Fog identity;
- FaceEye legacy shadow/environment;
- Bloom;
- HDR semantic bridge;
- DSR native SFX preservation boundary;
- DSR SfxPBL inverse-tonemap preservation.

Resource/material bridges are also explicit islands:

- Material Response routing;
- SpecRGB t10;
- Diffuse t0;
- Normal t2;
- Subsurface route.

## State policy

`BLOCKED` operators always fail open even when their feature gate is enabled.
`STOCK` host-preservation islands always preserve DSR.
`DIAGNOSTIC` operators require an explicit diagnostic opt-in.
Every other island still requires its declared verified receiver/material/resource/
producer/consumer gates.

No island is activated merely because it exists in this catalog.

## First source-complete operator primitive: terminal RGB SAT

`surface.terminal_sat_rgb` now has a handwritten, auditable local DXBC mutation
primitive in Renderer Core. It implements only the RE-confirmed terminal-output
translation:

```text
final separate RGB instruction word |= 0x00002000
```

The primitive is deliberately recipe-driven. The caller must supply the exact byte
offset and exact unsaturated instruction token recovered for an already verified
receiver. Before mutation it checks alignment, bounds, token identity and the explicit
`verified_separate_rgb_write` gate. Any mismatch fails open and leaves the shader
bytes unchanged. Reapplying the same recipe is idempotent.

This preserves the proven scope of the operator: the one-bit SAT modifier is
length-preserving and does not touch alpha on the supported separate RGB terminal
write. Combined RGBA terminal writes (including the known non-substantive/stub class)
are not accepted by this primitive because the same SAT modifier would clamp alpha;
those remain a separate diagnostic/body-rewrite problem.

This source addition closes the local mutation mechanism only. Exact shader/receiver
identity recipes still have to be imported or reconstructed under the source-complete
provenance rules before runtime activation. It does not claim whole-shader or
final-pixel equivalence.

## Source-complete semantic primitive: PntS attenuation

`surface.pointlight_pnts_attenuation` now has a handwritten operator-local forward
primitive matching the confirmed cross-render RE cut for the 72 substantive DSR PBL
HemEnv/HemEnvLerp PntS bodies:

```text
x = (End - distance) / (End - Begin)
DSR stock: A_D = sat(x^3)
PTDE target: A_P = sat(x)
```

The primitive computes both the retained host result and the PTDE target from the same
semantic inputs, and fails open on non-finite inputs or an invalid `End <= Begin`
range. It owns attenuation only. PointLight source RGB/intensity, authored Begin/End,
material response, local diffuse/specular equations and downstream composition are
not modified or compensated here.

PntSS/PntSSSS fixed families are deliberately outside this patch: RE shows their
local attenuation is already linear-x and therefore does not require this operator
translation. The remaining construction step before create-time runtime
materialization is importing/reconstructing the exact per-DXBC instruction recipes
for the certified PntS receiver set from the canonical P2.2 plan source.

This is a CONSTRUCTION-level operator implementation, not a claim of full PointLight
or final-pixel equivalence.

## Source-complete semantic primitive: Diffuse material-domain

`surface.diffuse_material_domain` now has a handwritten local DSR-host forward for
the certified material-domain mismatch:

```text
stock DSR local diffuse dependency: M_D = abs(Z_D)^2.2
bridge local diffuse dependency:    M_bridge = Z_D
PTDE reference operator:            M_P = Z_P
```

The important boundary is explicit: the bridge removes the DSR-local x^2.2 transform
from the targeted diffuse dependency while retaining the actual DSR pretransform
carrier. It does **not** assert `Z_D == Z_P`; texture, vertex and asset homology remain
separate questions. Blanket SPEC RAW and WORKFLOW RAW are not part of this island,
and atmosphere/postprocess are downstream owners rather than compensation surfaces.

The primitive fails open for non-finite input. Exact create-time DXBC recipe import is
still pending the canonical P2.2 plan source, so this closes the semantic/local-forward
construction layer rather than runtime activation or final-pixel equivalence.

## Source-complete semantic primitive: fixed-family post-Fog identity

`surface.fixed_postfog_identity` now has a handwritten exact forward for the
certified fixed PntSS/PntSSSS homolog family:

```text
DSR stock: Y = (s > 0.5) ? abs(C)^(1/2.2) : C
PTDE/bridge: Y = C
```

The bridge removes only the DSR conditional post-Fog root and therefore preserves
the signed post-Fog value on the PTDE path. Fog generation/mixing itself, upstream
material/light operators, terminal SAT and later HDR/postprocess remain separate
owners. The primitive fails open on non-finite inputs.

The canonical scope is 72 unique shader hashes / 144 substantive fixed-family aliases.
Exact per-DXBC create-time recipes are still pending import from the canonical P2.2
plan source, so this is a CONSTRUCTION/local-forward closure rather than runtime or
pixel-equivalence evidence.

## Source-complete semantic gate: certified no-Spc EnvSpec deletion

`surface.envspec_nospc_delete` now has an explicit fail-open semantic gate for the
confirmed 48 substantive PBL no-Spc homologs whose exact PTDE counterparts lack the
active EnvSpec lane:

```text
certified target: EnvSpec_term := 0
```

Deletion is authorized only when all four facts are already established for the
receiver: exact PTDE↔DSR homolog identity, substantive PBL no-Spc receiver class,
proven PTDE EnvSpec-lane absence, and alias-safe scope. Any missing condition preserves
the stock DSR EnvSpec term. This makes the broader-hash alias warning executable policy
rather than prose.

The island deletes only the DSR-only local EnvSpec contribution. It does not replace a
cubemap/probe, infer a material, alter diffuse/specular gains or claim final-pixel
equivalence. Exact DXBC identity/patch recipes remain pending import from the canonical
P2.2 plan source.

## Source-complete operator math: Upper/Lower Ambient

`surface.upper_lower` now has a handwritten exact implementation of the CONFIRMED
PTDE endpoint reconstruction, LightBank A/B blend and hemispheric source join:

```text
P(RGB,M) = (RGB / 255) * (M / 100)
U = (1-beta) U_A + beta U_B
L = (1-beta) L_A + beta L_B
t = 0.5 * N_final.y + 0.5
H = L + t * (U - L)
```

The reconstructed vectors correspond to the frozen carrier ABI semantics
`b13[6].xyz = Upper_PTDE` and `b13[7].xyz = Lower_PTDE`. The implementation is
deliberately PTDE-linear: it does not apply the stock DSR U/L x1.5, pow(2.2),
endpoint inverse/post-blend root, and it does not pre-distort the payload to cancel
the common downstream pre-Fog root.

This closes the operator mathematics and semantic payload construction only. The
runtime producer/selector/freshness sidecar remains a separate activation problem;
the historical A3 producer-hook failures therefore do not invalidate this pure
operator implementation and are not promoted to runtime success here.

## Legacy A1/P2.2 decomposition

The old combined mask is decomposed by ownership:

- 0x0001, 0x0008, 0x0100, 0x0200, 0x0800, 0x2000 -> terminal RGB SAT;
- 0x0002, 0x0040, 0x0400 -> diffuse material-domain;
- 0x0004 -> PntS attenuation;
- 0x0020 -> certified no-Spc EnvSpec deletion;
- 0x0080 -> fixed post-Fog identity;
- 0x4000 -> terminal RGBA SAT diagnostic.

A legacy plan with unknown bits is not eligible for automatic migration.

The old A1 executable plan payloads are not reclassified as one monolithic island.
When their exact byte-plan source is imported, each patch operation must be attached
to the island owning its semantic mask bit.


## Exact Expanded A1 identity migration

The historical Expanded A1 selection is reproduced from the canonical P2.2 release
catalog as 144 unique original DXBC identities / 252 shader aliases:

- 72 plans at mask 0x0008;
- 12 plans at mask 0x0027;
- 24 plans at mask 0x00C1;
- 36 plans at mask 0x0100.

Each identity records original SHA-256, expected replacement SHA-256, code size,
representative shader, alias count, legacy mask and island ownership. The generated
index is deterministic and CI-verified.

This is not yet a source-complete runtime byte patcher. The canonical 312 DWORD patch
operations are source-referenced to artifact 253,
`data/PORT_PLAN.json`, source-plan SHA-256
`5cc15f8084fb75cb33c27be3f0e94be7fd186f07c16274b33ee09735b747a1ec`.
Those operation bytes must be imported into Git/reconstructed and independently
verified before the 144-plan materializer can satisfy the project-wide release
source-completeness gate.

Until then the index is authoritative for identity, scope and operator ownership, not
authorization to activate the legacy replacement payload at runtime.
