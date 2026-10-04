# DSRRL P_Metal — HANDOFF 2026-10-04
## R6B / black armor / Firelink probe swap

### 0. START HERE

Repo: `julkszw-sys/DSRRL-Material-Response`

Current P_Metal baseline lineage:
- build key: `pmetal_full_ptde_hemenv_r6b_atmosphere_off`
- addon build id: `163`
- version: `R6B-ATMOSPHERE-OFF-2026-10-03`
- source branch: `fix/pmetal-r6b-atmosphere-off-20261003`
- source commit: `e37a4c73f7f19f4374af40fb782e10c182d241e6`
- construction: PASS
- compatibility/native build: PASS
- runtime/pixel for R6B: not promoted to PASS
- atmosphere/Fog/LightScattering override: **COMPLETELY REMOVED**
- Upper/Lower: OFF
- HemDir3: OFF
- Subsurface: OFF

Before any continuation:
1. `dsrrl.agent_bootstrap(topic)`
2. `dsrrl.renderer_addon_control_plane_status(relevant_build_key)`
3. obey `protect.renderer.pmetal.envspec_no_repeat_closed_diagnostics_v1`

Do not use old handoff claims as current pixel truth without checking current canonical findings. In particular, the old reflection handoff that treated Depths/Parish as a protected visual reference predates the current renderer bridge and is not sufficient authority for the current black-P_Metal residual.

---

## 1. CURRENT PIXEL STATE — OWNER CORRECTION

Latest owner observation on R6B:

- Firelink: P_Metal base remains black.
- Depths: P_Metal base also remains black.
- Earlier statement that Depths had a recovered/rich base reflection was **withdrawn by the owner**.
- Independent symptom: the **left side of every armor appears systematically darker**.

Treat this as **two potentially distinct residuals**:

### A. Black base/common energy residual
Possible remaining domains include:
- exact PTDE EnvDiffuse/common merge,
- LightBank `m/s` bank-selection equivalence,
- missing/incorrect common composition term,
- unresolved dynamic `k135` producer only if a mechanism connects it to the residual.

Do not fix with arbitrary gain.

### B. Persistent left/right directional bias
Possible remaining domains include:
- upstream `TEXCOORD3` / view-vector producer,
- PTDE cubemap face or in-face orientation under D3D9→D3D11 carriage,
- another directional scene term.

Do **not** patch the pixel-stage reflection formula or blindly flip an axis: that cut is now closed by exact RE.

Canonical updated finding:
- `renderer.pmetal.directional_surface_asymmetry_residual_v1`
- latest corrected revision: `10941`

---

## 2. PIXEL-STAGE REFLECTION VECTOR — CLOSED AS MISMATCH CANDIDATE

Exact RE confirms stable P_Metal PTDE and current DSR bridge use the same pixel-stage reflection equation:

`R = 2 * dot(N,V) * N - V`

PTDE exact homologs:
- 728 — Csd
- 741 — Sdw
- 754 — plain

DSR stable receivers:
- rx33 / shader 894 — reflection register `r7`
- rx34 / shader 913 — reflection register `r6`
- rx35 / shader 932 — reflection register `r5`

Important implementation detail:
`k_build131_window` contains a static placeholder coordinate in the literal template, but before materialization current code overwrites both PTDE cube sample-coordinate operands with `authority.reflection_coord_register`. Therefore active Build131 is not sampling EnvSpec with raw `V` or stale `r0`.

Canonical:
- `renderer.pmetal.reflection_vector_pixel_consumer_equivalence_v1`
- CONFIRMED
- revision `10942`

Remaining directional RE must move upstream/downstream:
- upstream producer of `TEXCOORD3` / V,
- cubemap face / in-face orientation,
- or another directional term.
Do not repeat direct-R/sample-register diagnostics already closed by PROTECT.

---

## 3. ATMOSPHERE / FOG — KEEP OFF

R6 reintroduced a historical V4B atmosphere-domain transform with an invalid carrier ABI.

Bad R6 state:
- current cumulative P_Metal runtime used `b12[0].xyz = c101,c101,c101`
- historical V4B transform interpreted `cb12[0].xyz` as encoded FogRGB
- runtime example had `c101 = 2.5`
- result: milky/white P_Metal with texture contrast loss

R6A first fail-opened that transform.
R6B then **physically removed it** by owner direction.

Do not:
- create a new FogRGB carrier,
- re-enable the V4B P_Metal atmosphere transform,
- use Fog/LightScattering to fix black armor.

Relevant source:
- R6B PR: `#222`
- commit: `e37a4c73f7f19f4374af40fb782e10c182d241e6`

---

## 4. ENVDIFFUSE — WHAT 1,1,1 ACTUALLY MEANS

For the observed Firelink row:
- live bank signature: `4c594553d201d80c`
- current mapping: `a10/m10_LightBank.param`
- row: `25`

Exact PTDE m10 row25 EnvDiffuse:
- raw RGB = `255,255,255`
- multiplier = `100`
- endpoint = `(1.0,1.0,1.0)`

So `(1,1,1)` is **not a clamp or decoder loss**. It is the exact authored PTDE m10 endpoint.

But PTDE s10 row25 is materially different:
- raw RGB = `220,250,255`
- multiplier = `300`
- endpoint ≈ `(2.5882, 2.9412, 3.0)`

Vanilla DSR values for comparison:
- m10 row25 ≈ `(1.7,1.7,1.7)`
- s10 row25 ≈ `(0.8627,0.9804,1.0)`

Open issue:
the bridge currently authenticates the **live DSR bank identity** and uses the corresponding PTDE donor bank/row. It is still not independently proven that original PTDE would select the same `m10` vs `s10` bank for the same physical draw/state.

Canonical:
- `renderer.pmetal.firelink_row25_envdiffuse_m10_s10_split_v1` — CONFIRMED
- `renderer.pmetal.cross_version_lightbank_bank_selection_equivalence_v1` — OPEN

Do not replace m10 with s10 or multiply EnvDiffuse arbitrarily until the bank-selection producer/routing is closed.

---

# 5. CRITICAL NEXT DIAGNOSTIC — CATACOMBS PROBE INTO FIRELINK

This is the step the owner explicitly expected and must not be skipped again.

The diagnostic is **already implemented and built**. Do not recreate it.

### Source/target probe

Authenticated source:
- ordinal `70`
- logical probe: `GI_EnvDif_m10_02_0000`
- Firelink

Forced target:
- ordinal `173`
- logical probe: `GI_EnvDif_m13_00_0001`
- Catacombs

Implementation branch:
- `diag/pmetal-probe-swap-firelink70-catacombs173-20261004`

Commit:
- `0fd4aa059e5e05fffdb16aef380a41a6f03a0640`

PR:
- `#223`

Windows workflow:
- run `37173359453`
- construction PASS for all three modes

The swap is narrow:
1. it first authenticates the real live stock probe normally;
2. it only triggers when stable HemEnv resolves exact source ordinal 70;
3. all unrelated probes remain unchanged;
4. it does not substitute LightBank/material state;
5. it is not a production routing shortcut.

### Three already-built variants

#### A. EnvSpec only
Build key:
`pmetal_probe_swap_firelink70_catacombs173_envspec`

Behavior:
- PTDE EnvSpec probe 70 → 173
- PTDE EnvDiffuse remains probe 70

Build id: `164`
Artifact:
`DSRRL_PMETAL_PROBE_SWAP_FIRELINK70_CATACOMBS173_envspec`
Artifact id: `11291973284`
ZIP/artifact digest:
`sha256:8f92da8a4634827f50738918e1e23bb48a1f567c6208dfb0c76158b246fbff31`
Addon SHA256:
`1de50aba3c32255df42a12ac71abc3dc314116838484479b6a7f286582a532a4`
Construction/native: PASS
Runtime/pixel: NOT TESTED / OPEN

#### B. EnvDiffuse only
Build key:
`pmetal_probe_swap_firelink70_catacombs173_envdiffuse`

Behavior:
- PTDE EnvSpec remains probe 70
- PTDE EnvDiffuse probe 70 → 173

Build id: `165`
Artifact:
`DSRRL_PMETAL_PROBE_SWAP_FIRELINK70_CATACOMBS173_envdiffuse`
Artifact id: `11292072802`
ZIP/artifact digest:
`sha256:7ce3b265bfb45ea132fc459705a95de0408ce51d36cfd9ac36b3a1eb8cd44967`
Addon SHA256:
`98acb64729e1c08ff82dc182c4be24651f0b481ea5b909e58303805f14ea00c0`
Construction/native: PASS
Runtime/pixel: NOT TESTED / OPEN

#### C. EnvSpec + EnvDiffuse
Build key:
`pmetal_probe_swap_firelink70_catacombs173_both`

Behavior:
- both exact PTDE EnvSpec and EnvDiffuse probe 70 → 173

Build id: `166`
Artifact:
`DSRRL_PMETAL_PROBE_SWAP_FIRELINK70_CATACOMBS173_both`
Artifact id: `11292646222`
ZIP/artifact digest:
`sha256:229ef2a601a6f8b301fb3ec5e7abab118f4edb94f761c89c8140239ebae64ac6`
Addon SHA256:
`4f9cbc3041fe1b571e972e2ee60e791f3ba584b14156383f4f45fef221d75106`
Construction/native: PASS
Runtime/pixel: NOT TESTED / OPEN

### How to interpret the probe-swap diagnostic

If EnvSpec-only materially changes the black/reflection phenotype:
- resource content / spatial EnvSpec field is decision-relevant.
- continue with exact probe mapping/content/orientation, not global gain.

If EnvDiffuse-only materially changes black base:
- diffuse environment field is decision-relevant.
- then inspect EnvDiffuse resource + common merge separately from EnvSpec.

If only BOTH changes it:
- interaction/common composition is likely important; avoid attributing to either resource alone.

If none changes the black base:
- the black residual is downstream/upstream of probe content itself.
- prioritize common merge / bank selection / missing PTDE scene term.
- for the left/right bias continue `TEXCOORD3` producer and cube orientation RE.

The project-wide no-new-runtime-dependency rule still applies:
the solver must not *require* a new user test to proceed. These builds are already-constructed falsifiers; continue static RE regardless.

---

## 6. CURRENT R6B CUMULATIVE CONTENT

Retained:
- full PTDE stable HemEnv EnvSpec carrier + RGB/A decode
- full PTDE stable HemEnv EnvDiffuse PackedGI carrier + RGB/A decode
- R3: remove DSR-only postmerge environment visibility scalar
- R4: direct PTDE SpecRGB RGB consumer
- R5: PTDE normal basis
  - no DSR front-face N sign branch
  - per-pixel `B = normalize(cross(N0,T) * handedness)`
- PHN scene-encoding/k135 terminal consumer
  - current dynamic producer remains OPEN
  - unity fail-open, no hardcoded 0.5
- exact equipment diffuse/normal authority where attested

Off:
- Upper/Lower
- HemDir3
- Subsurface
- P_Metal atmosphere/Fog/LightScattering override

Open:
- exact dynamic DrawEnv/ToneMap producer for `k135`
- exact PTDE `Visibility_P` for Csd/Sdw at its verified position
- complete HemEnvLerp parity
- cross-version LightBank m/s selection equivalence
- upstream TEXCOORD3/view producer equivalence
- cubemap face/in-face orientation
- final black-base common-merge residual

---

## 7. DO NOT REPEAT CLOSED PMETAL ENVSPEC TESTS

PROTECT:
`protect.renderer.pmetal.envspec_no_repeat_closed_diagnostics_v1`

Closed unless a real semantic lineage change formally reopens them:
- invalid endpoint guard
- COLOR0-only neutralization
- sampler/mip/LOD-only
- direct-R LOD0 RGBA source-only
- R11F carrier-only
- PackedGI byte/alpha/order/index
- stable t11 missing/clobbered
- PR209 host postmerge visibility bypass
- native DSR mip0/mip3 direct PTDE carrier
- k135-only hybrid
- old native `gFC_EnvSpcMapMulCol` restore
- Build102/103 component black views are non-diagnostic

Allowed next under current PROTECT:
- exact PTDE EnvDiffuse/common merge work
- exact PTDE `Visibility_P` at verified position
- genuinely new source semantic cut with new evidence

The Firelink70→Catacombs173 forced-probe diagnostic is already constructed as a narrow resource-substitution semantic cut. Do not duplicate it.

---

## 8. SPECRGB REJECTS

Do not misread `fail_spec_rgb=1` as whole-bridge failure.

Observed R6 runtime:
- candidates: `11458`
- accepted requests: `8064`
- SpecRGB rejects: `3394`

These are draw-local exact-resource fail-open outcomes from the existing contract. R5→R6 did not introduce that reject path. A rejected draw must remain stock/fail-open rather than receiving a partial PTDE SpecRGB hybrid.

This is separate from the old R6 milky-white regression, whose confirmed cause was the atmosphere cb12 ABI collision.

---

## 9. EXACT STABLE HEMENV RECEIVERS

DSR:
- rx33 index 894  
  `FRPG_Phn_DifSpcBmp______Csd_HemEnv.fpo`  
  SHA256 `35880c0b2f2330208dfc21af6dd3d944218fcc4540cd8e59404a0aefc13c0b24`

- rx34 index 913  
  `FRPG_Phn_DifSpcBmp______Sdw_HemEnv.fpo`  
  SHA256 `d6038de494509e7cbcbfb904c4046e9427f3b921f6a35735a0b0d316f9976837`

- rx35 index 932  
  `FRPG_Phn_DifSpcBmp__________HemEnv.fpo`  
  SHA256 `7d03c75b69f5730eb741a4d327189d0bbed8a8450fb0ac04e1505f7b91763701`

PTDE homologs:
- 728 SHA256 `0c6f2982bc36d56151791acd03caffb63358324d746f3dd408cfeda976fb2daf`
- 741 SHA256 `9e931f72ca7ebe7ae5bf68f23ff8a7ff3a818f314fd2578f47b1315e9331d9a6`
- 754 SHA256 `77b40333a33f5543149a7b9ecd371da23af38febbd149b151e429d38a4c2b836`

Plain rx35 has no Csd/Sdw `Visibility_P`.

---

## 10. NEXT AGENT — PRIORITY ORDER

1. **Do not rebuild the Catacombs→Firelink probe swap. It already exists in three modes under PR223.**
2. Verify current control plane for builds 164/165/166.
3. Continue static RE even if no new runtime result is available.
4. For black base:
   - close exact PTDE EnvDiffuse/common merge,
   - close `m10/s10` bank-selection producer equivalence,
   - do not tune amplitude empirically.
5. For left-side dark bias:
   - RE the producer of `TEXCOORD3`/V in the paired PTDE/DSR vertex path,
   - audit PTDE cubemap raw face order and per-face orientation versus D3D11 materialization,
   - only then consider a coordinate transform.
6. Keep PTDE `Visibility_P` separate and exact:
   - Csd/Sdw only,
   - at verified pre-material location,
   - do not conflate with the already-removed DSR postmerge scalar.
7. Translate confirmed discoveries into main/cumulative addon code, not only reports.
8. Before finishing:
   - submit evidence,
   - propose/promote only justified findings,
   - verify revision/bootstrap,
   - run `dsrrl.knowledge_hygiene_audit()`,
   - re-check renderer addon control plane.

Current Supabase revision observed during handoff creation: `10958`.