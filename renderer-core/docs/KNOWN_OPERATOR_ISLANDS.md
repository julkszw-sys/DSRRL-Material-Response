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
