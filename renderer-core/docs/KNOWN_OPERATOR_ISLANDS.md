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
- terminal RGBA SAT generic translator — REJECTED/BLOCKED;
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

`REJECTED` canonical operators and `BLOCKED` operators always fail open even when their feature gate is enabled.
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

## Source-complete routing contract: PTDE SpecRGB split resource

`bridge.specrgb` now has an explicit FULL24 routing gate for the confirmed stable
no-PointLight Phn Spc HemEnv alternate receiver set:

```text
receivers 24..35 -> DifSpcBmp
receivers 36..47 -> DifSpc
PTDE SpecRGB.rgb -> independent t10
stock DSR t1 -> preserved unchanged, including alpha/roughness
```

Receiver identity alone is intentionally insufficient. Activation also requires an
exact actual-material route with a verified specular consumer, exact-name PTDE
SpecRGB companion identity, a ready sidecar, native t10 transport, and proof that
stock t1 remains intact. Missing any coordinate fails open to stock DSR. This keeps
shared materials on the existing `PTDE_COMPANION_REQUIRED` policy and naturally
excludes nonhomologous/no-spec routes such as `P[D].mtd`.

This closes the bridge's core routing policy, not the resource-loader implementation
or per-route runtime attestation. The existing FULL24/Spec-only lineage remains
provenance for those later integration layers.

## Source-complete routing contract: PTDE equipment Normal

`bridge.normal_resource` now has an explicit conservative routing contract for the
stable no-PointLight DifSpcBmp HemEnv receiver family 24..35. The bridge may replace
only the actually bound DSR `t2/g_Bumpmap` resource, and authorization must come from
one of two already-certified semantic cuts:

1. an exact homologous PTDE Bmp material route; or
2. a pre-certified exact application-bound `t0+t1+t2` resource-signature tuple.

Both paths additionally require an unambiguous logical target, exact PTDE normal
sidecar identity and a ready PTDE SRV. The host `s2` sampler contract is preserved;
the core does not guess a replacement sampler. Any mismatch fails open to stock DSR
`t2`.

Filename-derived `_s -> _n` inference is intentionally absent because retained FLVER
evidence falsifies that shortcut. DSR-added NONHOMOLOGOUS bump routes such as
`P[D].mtd` and `P_Leather[DS].mtd` therefore receive no authorization.
`Ps_Body[DSBT].mtd` remains a separate Subsurf path and non-stable receivers are
outside this island.

The prior V12 single-addon lineage already demonstrated runtime liveness and t2
activation for the tested ordinary DifSpcBmp equipment path. This core addition
reconstructs the routing policy source-completely; it does not claim that the new
Renderer Core integration has itself been runtime-tested or that PTDE pixel
equivalence is closed.

## Source-complete routing contract: PTDE equipment Diffuse

`bridge.diffuse_resource` now models the complete narrow Diffuse augmentation for the
stable no-PointLight DifSpcBmp HemEnv receiver family 24..35. Raw PTDE diffuse texture
replacement is explicitly insufficient. Activation requires the three already-proven
coordinates to converge on the same draw:

```text
exact PTDE diffuse companion -> t0
exact paired PTDE c100 material donor
certified diffuse-linear Material Response receiver
```

The gate also requires verified actual material and actually bound DSR `t0` identity.
Shared-MTD routes retain the exact texture-identity conjunction rather than using the
MTD name alone.

Failure is intentionally `preserve_existing_route`, not "turn everything stock".
That distinction preserves a separately valid SpecRGB/SPEC_ONLY route when an exact
PTDE diffuse companion is absent. Once all Diffuse coordinates are ready, dispatch
moves to the existing full c100+c101 material-response semantics; no new diffuse shader
family or cross-operator compensation is introduced.

The prior V12 single-addon lineage demonstrated end-to-end `t0` activation for the
tested ordinary DifSpcBmp equipment path. This commit reconstructs the routing policy
inside Renderer Core; new-core runtime activation and PTDE pixel equivalence remain
separate validation layers.

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

The exact patch recipe source has now been recovered from artifact 253 itself.
The original recovered P2.2 plan was verified at source-plan SHA-256
`5cc15f8084fb75cb33c27be3f0e94be7fd186f07c16274b33ee09735b747a1ec`
(518 plans / 1436 DWORD operations). The exact A1 subset required by this core is
stored directly in Git as three plaintext TSV parts under
`renderer-core/data/provenance/a1_exact_recipes_v1.part*.tsv`.

The concatenated recipe corpus SHA-256 is
`c38d5560e7dfc99f149da0908d754400e8bf654fe7e0a0469e5f08f47d7575b0`.
CI verifies the exact selected subset against the identity index:
144 plans / 252 aliases / 312 DWORD operations, including every `byte_offset`,
`old`, `new`, replacement SHA, code size and mask.

This removes the former external-artifact blocker for exact recipe provenance.
Runtime materialization is still a separate implementation/activation layer: the
presence of exact offsets in Git does not by itself authorize the legacy combined
replacement payload or promote runtime/pixel status.


## RE closure: generic combined-RGBA terminal SAT is rejected

Canonical revision 8857 closes the weakest historical island by falsification rather
than by expanding its patch scope. Independent re-parse of source artifact 99
(`DSRRL_FINAL_EVIDENCE_PART02(2).zip`, SHA-256
`57314b36812dfa2867512052278a08dc0da8a41ea0414c1fbbe4ccdf45838076`)
covers all 384 recovered exact-name PTDE `FRPG_Phn` HemEnv/HemEnvLerp shaders:

- 96 BASE, 96 PntS, 96 PntSS and 96 PntSSSS;
- 384/384 last `COLOROUT0` writes use RGB mask `0x7` with SAT;
- 0/384 use a combined RGBA terminal write;
- the recovered PTDE Phn HemEnv corpus contains no exact-name `Alp`,
  `Parallax` or `Subsurf` variants.

The historical 0x4000 class selected DSR combined-RGBA writes. Setting SAT on that
instruction therefore clamps alpha in addition to RGB, which is not the recovered
PTDE Phn terminal operator. The same historical hash class also carries 120
byte-identical collateral aliases, including DSR-only Parallax/Subsurf/Phn-Alp
names; byte identity is not semantic authorization.

Consequences for Renderer Core:

- `surface.terminal_sat_rgba` is canonical `REJECTED` and `BLOCKED`;
- legacy mask bit `0x4000` is a rejected migration bit;
- diagnostic opt-in cannot activate the generic island;
- `surface.terminal_sat_rgb` is unchanged and remains the correct primitive for
  verified separate RGB terminal writes;
- a future combined-write receiver can reopen only through a narrow body rewrite
  that saturates RGB while preserving alpha, or direct alpha-equivalence proof for
  one exact gated subclass.

Reproducible audit metadata is stored in
`data/audits/terminal_sat_rgba_revalidation_rev8857.json`; the parser is
`tools/audit_ptde_phn_terminal_write.py`.


## Ps_Body Subsurface: exact create-time bypass carrier

The current Ps_Body target remains a **HIGH CONFIDENCE** semantic route: exact DSR
`Ps_Body[DSBT].mtd` maps to PTDE `Ps_Body[DSB].mtd`, whose target surface is the
ordinary plain `ColDifSpcBmp` path rather than DSR's added Subsurf/SSS operator.

Revision 8873 separately closes the **carrier** at CONSTRUCTION level. The three
certified stable no-PointLight source bodies map variant-preservingly:

| Source Subsurf | Target ordinary | Core receiver |
| --- | --- | ---: |
| `FRPG_Phn_DifSpcBmp______Csd_HemEnvSubsurf.fpo` | `FRPG_Phn_DifSpcBmp______Csd_HemEnv.fpo` | 33 |
| `FRPG_Phn_DifSpcBmp______Sdw_HemEnvSubsurf.fpo` | `FRPG_Phn_DifSpcBmp______Sdw_HemEnv.fpo` | 34 |
| `FRPG_Phn_DifSpcBmp__________HemEnvSubsurf.fpo` | `FRPG_Phn_DifSpcBmp__________HemEnv.fpo` | 35 |

Pairwise DXBC RE proves, for all 3/3 pairs:

- ISGN is byte-identical;
- OSGN is byte-identical;
- all five constant-buffer semantic layouts are identical;
- shader-model token is identical;
- the ordinary target resource set is a strict subset of the Subsurf source;
- the only removed declarations are `t10 gSMP_10` and `s10 gSMP_10Sampler`.

Therefore the narrow bypass carrier is create-time pixel-shader substitution/reuse;
no EXE hook, draw replay, vertex-stage rewrite, output-stage rewrite or expanded host
resource binding is required for the bypass itself.

This does **not** authorize a bare Subsurf->vanilla-plain swap. The route remains
fail-open until exact material/donor/body/draw identity and the complete ordinary PTDE
SpecRGB + Diffuse + Normal + Material Response target are ready. Runtime activation
of this Renderer Core path and PTDE-visible pixel equivalence remain open.

The reproducible binary audit is
`tools/audit_subsurface_plain_bypass_abi.py` with compact provenance in
`data/audits/subsurface_plain_bypass_abi_v1.json`.
