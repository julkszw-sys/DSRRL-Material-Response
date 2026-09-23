# PTDE Renderer Full Reconstruction — RE Research Log

Branch: `dev/ptde-renderer-full-reconstruction`

## Persistence policy

Primary knowledge store: Supabase `dsrrl`.

If Supabase bootstrap, evidence submission, finding promotion, schema access, or control-plane access is unavailable or fails in a way that blocks research, **research must continue** and be persisted here immediately.

Fallback protocol:

1. Append raw RE evidence here first.
2. Preserve exact provenance: binary/artifact, SHA-256 when known, addresses/callsites, shader/resource identities, method, and confidence.
3. Separate observation from interpretation.
4. Use project statuses only: CONFIRMED / HIGH CONFIDENCE / HYPOTHESIS / OPEN / REJECTED.
5. Do not overwrite or delete superseded results; append falsifiers and mark old models REJECTED/SUPERSEDED.
6. Once Supabase is available again, backfill in order:
   - `dsrrl.submit_evidence(...)`
   - `dsrrl.propose_finding(...)`
   - `dsrrl.promote_finding(...)` when justified
   - producer/operator mapping tables
   - revision/bootstrap verification
   - `dsrrl.knowledge_hygiene_audit()`
7. GitHub fallback notes remain as audit history after backfill.

## Scope

Target: reconstruct PTDE renderer producers/operators on the DSR host, excluding DSR-added SFX that are intentionally retained.

Required model for each operator:

`source -> producer -> transport -> consumer -> composition -> postprocess -> pixel`

Producer/CB/resource equivalence is not pixel equivalence.

## Current branch state

Created from `nexus-review/material-response-1.45-source`.

### CONFIRMED — PTDE ShaderConstant producer ABI census

Direct binary RE of canonical PTDE `DATA.exe` recovered registered `ShaderConstant_*Entity` factory/getter/constructor/vtable chains and entity-defined managed-block footprints:

| Producer | Size |
|---|---:|
| CameraMtx | 0xA0 |
| ClipPlane | 0xC0 |
| DirLight | 0x3B0 |
| DofParam | 0x460 |
| Fog | 0x90 |
| LightScattering | 0xF0 |
| LocalWorldMtx | 0x780 |
| LocalWorldMtx_WithPrev | 0xEE0 |
| Normal2AlphaParam | 0x70 |
| PointLight | 0xE0 |
| ToneMap | 0x70 |
| Water | 0x1B0 |
| WorldViewClipMtx | 0xE0 |

Important: these values are **not yet claimed as GPU CB sizes**. RE already shows the virtual slot does not describe the object instance size itself. Its exact managed-block semantics remain under investigation.

### CONFIRMED — producer-class set divergence

Normalized class census:

PTDE-only:
- `ShaderConstant_ClipPlaneEntity`

DSR-only:
- `ShaderConstant_DeferredLightingEntity`
- `ShaderConstant_GlowEntity`
- `ShaderConstant_WindParamEntity`

This proves compiled producer-class divergence only. Runtime activation and visible responsibility are still OPEN until consumer/routing RE closes them.

### CONFIRMED — HemDir3 stock DSR execution graph issue

DSR retains HemDir3 shader payloads, but current stock shader registration/binding evidence indicates the family is orphaned/unreachable in the audited ordinary stock PHN execution graph. Therefore restoring PTDE HemDir3 behavior may require restoring the execution/receiver path as an operator island, not merely reproducing LightBank producer values.

## Current RE target

Resolve the semantic meaning and downstream consumer of the producer virtual slot that returns large PTDE blocks (e.g. PointLight 0xE0, DirLight 0x3B0, ToneMap 0x70) but often 0x20 in DSR. Do not label it constant-buffer size until its allocator/copy/upload callsites prove that interpretation.

---

## 2026-09-23 — PTDE PHN FaceEye shader-math census

Status: **CONFIRMED PTDE math / pending Supabase backfill**

Corpus: 12 original PTDE `FRPG_Phn_FaceEye_*.fpo` shaders from `DSRRL_FINAL_EVIDENCE_PART02(2).zip`. Static ps_3_0 disassembly covers no-shadow, Sdw and Csd variants, each with 0/1/2/4 PointLights. DSR active-host counterparts are binder indices 1636..1647; at research start those FaceEye shader IDs had no rows in `dsrrl.shader_math_bindings`.

### FaceEye base/material operator

Let `N=normalize(normal)`, `V=normalize(view)`, `R=2*dot(N,V)*N-V`, and `t=0.5*N.y+0.5`.

PTDE probe decode:
`EnvDifProbe = texCube(s11,N).rgb / texCube(s11,N).a`
`EnvSpcProbe = texCube(s12,R).rgb / texCube(s12,R).a`

Hemisphere/environment:
`H = c99.rgb + t*(c98.rgb-c99.rgb)`
`EnvDiffuse = H + c86.rgb*EnvDifProbe`
`EnvSpec = c87.rgb*EnvSpcProbe`

FaceEye-specific diffuse shaping:
`Td=tex2D(s0,uv)`
`FaceBase = Td.rgb*(1 + Td.a*(c174.rgb-1)) + c156.rgb`
`DiffuseMaterial = FaceBase*c100.rgb*VertexColor.rgb`

Specular material:
`Ts=tex2D(s1,uv)`
`SpecMaterial = Ts.rgb*c101.rgb*VertexColor.rgb`

No-Point pre-fog surface:
`SurfaceRGB = DiffuseMaterial*EnvDiffuse + SpecMaterial*EnvSpec`.

Higher-level authored names of `c156/c174` remain OPEN; their shader math is exact.

### Local PointLight operator

For each PTDE FaceEye PointLight:
`D=P_i-P`, `d=length(D)`, `L=D/d`
`A=saturate((End_i-d)*InvSpan_i)`
`Light=A*Color_i`

Carriers: `P_i.xyz=c112..c115.xyz`, `InvSpan_i=c112..c115.w`, `Color_i.rgb=c116..c119.rgb`, `End_i=c116..c119.w`.

Diffuse:
`PointDiffuse=max(dot(N,L),0)*Light`

Legacy local specular:
`PointSpec=pow(max(dot(R,L),0),c102.x)*Light`

All PTDE FaceEye PntS/PntSS/PntSSSS variants use **linear attenuation**. Existing DSR consumer census classifies FaceEye PntS as `CUBIC_X3_SPECIAL` and FaceEye PntSS/PntSSSS as `LINEAR_X_SPECIAL`. Therefore no blanket FaceEye PointLight patch is authorized; the narrow known mismatch is PntS `x^3 -> x`, subject to exact receiver/material routing.

### Sdw / Csd shadow operator

Both use 16-tap 4x4 PCF. Packed depth decode is:
`Depth(S)=dot(S.rgb,(255/256,255/65536,255/16777216))`.

Tap offsets are the Cartesian grid of `{-3,-1,+1,+3}/4096`.
`PCF=(1/16)*sum(Depth(S_j)<receiverDepth)`.

Common colored shadow response:
`Bias=saturate((dot(c175.xyz,N)+c121.x)*c121.w)`
`ShadowAmount=saturate(PCF+Bias)`
`ShadowFade=saturate((c121.y-length(viewVector))*c121.z)`
`ShadowRGB=1-ShadowAmount*ShadowFade*c122.rgb`.

Sdw uses the projected input plus `c157` atlas/wrap correction. Csd selects one of four cascade transforms from `c140..c155` via `c123`, selects atlas data from `c157..c160`, then executes the same packed-depth PCF and ShadowRGB law.

Critical operator boundary, verified in SdwPntS/PntSS/PntSSSS and CsdPntS/PntSS/PntSSSS:
`ShadowRGB` multiplies **EnvDiffuse probe and EnvSpec probe terms only**. Upper/Lower hemisphere `H` is added unshadowed, and local PointLight diffuse/specular contributions are also added outside this shadow multiplier.

Thus shadow/environment and PointLight are independent bridge coordinates for FaceEye.

### Terminal

After the common PHN fog/scattering tail all 12 end in terminal RGB SAT through `c135.x/c135.y`, consistent with the established PTDE PHN scene encoding.

### Bridge impact

1. PntS attenuation bridge may be required for DSR FaceEye, but PntSS/PntSSSS must not inherit that patch.
2. FaceEye material/BRDF response should be compared independently against DSR before porting.
3. Shadow Sdw/Csd must remain isolated from local PointLight because PTDE does not use the shadow map to gate those local lights.
4. EnvDiffuse and EnvSpec remain separate legacy channels; their resource/consumer bridges must preserve the shadowing placement above.
5. Exact FaceEye receiver identity is mandatory; fail-open for non-FaceEye draws.

Next FaceEye RE target: exact DSR FaceEye material/BRDF equation and PTDE↔DSR delta beyond the already-censused PntS attenuation mismatch.


---

## 2026-09-23 — PTDE PHN Dep / DepAlp / Vel / VelAlp shader math

Status: **CONFIRMED PTDE math / pending Supabase backfill**

Corpus: the five PTDE PHN special-output shaders outside the main Hem*/FaceEye material families:
- `FRPG_Phn_Dif________________Dep.fpo`
- `FRPG_Phn_Dif________________DepAlp.fpo`
- `FRPG_Phn_Dif______Mul_______DepAlp.fpo`
- `FRPG_Phn_Dif________________Vel.fpo`
- `FRPG_Phn_Dif________________VelAlp.fpo`

### Depth producer: exact RGB packing

Let:
`d = v0.x / v0.y`
`K = 255.99998474121094`
`T(x) = truncation toward zero` as implemented by the shader's FRC + CMP correction.

Then:
`a = K*d`
`i0 = T(a)`
`f0 = a-i0`
`b = 256*f0`
`i1 = T(b)`
`f1 = b-i1`

The PTDE depth shader writes:
`R = i0/255`
`G = i1/255`
`B = 256*f1/255`

Thus:
`DepthRGB(d) = (i0/255, i1/255, 256*f1/255)`.

For the ordinary non-negative projective depth domain, `T` reduces to floor.

### Exact producer -> FaceEye shadow consumer closure

The FaceEye Sdw/Csd consumer previously recovered:
`Decode(RGB)=dot(RGB,(255/256,255/65536,255/16777216))`.

Substituting the producer output gives:
`Decode(DepthRGB(d)) = K*d/256`
`= 0.9999999403953552*d`.

So the PTDE Dep producer and PHN shadow PCF consumer form an explicit matched transport pair. The tiny scale below 1 is not an arbitrary bridge gain; it follows directly from the compiled `K=255.99998474121094` constant.

This closes a source -> encoding -> texture transport -> shadow consumer segment of the PTDE forward.

### Depth alpha semantics are route-specific

`FRPG_Phn_Dif________________Dep.fpo`:
`oC0.a = 1`.

`FRPG_Phn_Dif________________DepAlp.fpo`:
`oC0.a = tex2D(s0,uv).a * c100.w * v1.w`.

`FRPG_Phn_Dif______Mul_______DepAlp.fpo`:
the pixel shader performs no diffuse texture sample and writes
`oC0.a = c100.w`.

Therefore `DepAlp` is not one universal alpha formula; the exact material/shader route must select the correct alpha behavior.

### Velocity producer

`FRPG_Phn_Dif________________Vel.fpo`:
`oC0.xyz = v0.xyz`
`oC0.a = 1`.

The pixel shader performs no velocity computation. Its RGB is an upstream varying payload, so reconstruction of the actual motion-vector equation belongs to the vertex/producer path rather than this pixel shader.

`FRPG_Phn_Dif________________VelAlp.fpo`:
`oC0.xyz = v2.xyz`
`oC0.a = tex2D(s0,v1.xy).a * c100.w * v0.w`.

Again the velocity RGB is upstream; the pixel shader only applies the material alpha mask.

### Bridge impact

1. PTDE shadow-depth compatibility cannot be reduced to a generic depth texture replacement; the exact packed RGB representation and its consumer decode are a matched operator pair.
2. Any PTDE shadow island on DSR must either preserve this pair or convert explicitly at the semantic cut. Do not inject packed PTDE depth into a DSR consumer that expects a different representation.
3. DepAlp alpha behavior is receiver/material-route specific and must fail-open when the exact shader route is not known.
4. Vel/VelAlp pixel shaders do not reveal the PTDE motion-vector equation; next RE must move upstream to the vertex/varying producer before a velocity bridge can be canonical.
5. The depth producer math is sufficiently closed to serve as a reference formula in the shader-math table.

Next target: identify the DSR counterparts' depth/velocity representation and compare the semantic cut, then continue to another PTDE legacy material family with incomplete math.


---

## 2026-09-23 — correction: DLRuntimeClass slot semantics

Status: **CONFIRMED; supersedes the earlier "managed-block/payload footprint" interpretation above**

Direct RTTI/reflection RE proves the vtable previously followed belongs to `DLRF::DLRuntimeClassImpl<T>`, not the runtime `ShaderConstant_*Entity` object vtable. Embedded provenance points to `ClassLibrary/Core/Reflection/Source/DLRuntimeClass.cpp`.

The relevant virtual slot returns **`sizeof(T)` for the reflected class**. Primitive template specializations prove the semantic directly:

- signed char -> 1
- short -> 2
- int / float -> 4
- int64 / double -> 8
- bool -> 1

Therefore values such as PointLight `0xE0`, DirLight `0x3B0`, ToneMap `0x70`, etc. are reflected **class instance sizes**, not GPU CB sizes and not staging-payload sizes.

Numeric census remains valid; its earlier semantic label is rejected.

## 2026-09-23 — CONFIRMED PTDE shader-float state domains

Direct D3D9 dispatch closes the state-bank identity:

- context `+0x10` -> VS float state -> flush `0x00431650` -> `IDirect3DDevice9::SetVertexShaderConstantF`
- context `+0x1050` -> PS float state -> flush `0x00431760` -> `IDirect3DDevice9::SetPixelShaderConstantF`

Register numbers are therefore always stage-qualified. `VS c135` and `PS c135` are unrelated slots.

## 2026-09-23 — CONFIRMED exact producer transports

### PointLight

`ShaderConstant_PointLightEntity::update 0x0042A100` / helper `0x00F902F0`:

- `entity+0x60+0x10*i -> PS c112+i`
- `entity+0xA0+0x10*i -> PS c116+i`
- `i=0..3`

This closes the four fixed legacy PointLight slots.

### ToneMap / terminal c135

`ShaderConstant_ToneMapEntity::update 0x0042B0E0`:

- `entity+0x60 -> PS c135`

Upstream DrawEnv/ToneMap provenance into `entity+0x60` remains OPEN.

### Fog

`ShaderConstant_FogEntity::update 0x00427210`, helper `0x00F92B60`:

- `entity+0x70 = Begin`
- `entity+0x74 = Range=End-Begin`
- `entity+0x78 = 0` on ordinary path
- `entity+0x7C = strength`
- these form `VS c128=(Begin, Range!=0 ? 1/Range : 0, 0, strength)`
- `entity+0x80 -> PS c103=(FogRGB,strength)`

### LightScattering

Builder `0x00FF2F90`, basis constructor `0x00FF34F0`, entity setter/update `0x00428340 / 0x00428330`:

| lane | semantic | PS | VS |
|---:|---|---|---|
| 0 | Beta1PlusBeta2 | c104 | c129 |
| 1 | TerrainReflectance | c105 | c130 |
| 2 | OneOverBeta1PlusBeta2 | c106 | c131 |
| 3 | HGg | c107 | c132 |
| 4 | BetaDash1 | c108 | c133 |
| 5 | BetaDash2 | c109 | c134 |
| 6 | SunColor.rgb + blendCoef/100 in w | c110 | c135 |
| 7 | LightDir.xyz + distanceMul/100 in w | c111 | c136 |

`SunColor.rgb=(SunRGB/255)*(SunA/100)`.

`LightDir.xyz=(cos(rx)*sin(ry), -sin(rx), cos(rx)*cos(ry))`, with authored rotations converted from degrees to radians.

### Camera / WorldViewClip

- CameraMtx `entity+0x60 -> transform -> VS c4-c7`
- WorldViewClipMtx `entity+0x60 -> transform -> VS c0-c3 + PS c165-c168`

## 2026-09-23 — CONFIRMED DirLightEntity transport map

`ShaderConstant_DirLightEntity::update 0x00424060` is the central legacy surface-state carrier.

Known semantic core:

- `+0x70/+0x80/+0x90 -> PS c92/c93/c94`: D1-D3 direction
- `+0xA0/+0xB0/+0xC0 -> PS c95/c96/c97`: D1-D3 color
- `+0xD0 -> PS c98`, `+0xE0 -> PS c99`: Upper/Lower
- `+0x110 -> PS c86`: EnvDiffuse A
- `+0x120 -> PS c87`: EnvSpec A
- `+0x130 -> PS c84`: EnvDiffuse B
- `+0x140 -> PS c85`: EnvSpec B
- `+0x150/+0x160/+0x170 -> PS c100/c101/c102`: diffuse material / specular material / legacy specular exponent
- `+0x180 -> PS c139`: model/common material multiplier
- `+0x380 -> PS c156`: additive diffuse/skin carrier

Additional transport is directly mapped but semantic naming remains OPEN for parts of it:

- `+0xF0/+0x100 -> PS c88/c89`
- `+0x190/+0x1A0/+0x1B0 -> VS c246-c248`
- transformed `+0x1C0 -> VS c137-c140`
- four transformed matrices `+0x200/+0x240/+0x280/+0x2C0 -> PS c140-c155`
- `+0x300/+0x310/+0x320/+0x330 -> PS c121/c122/c123/c175`
- `+0x340..+0x370 -> PS c157-c160`
- `+0x390 -> PS c174`
- `+0x3A0 -> PS c182`

Do not assign meanings to the OPEN auxiliary registers without consumer/callsite proof.

## Current next target

Close the remaining DirLight auxiliary lanes and then continue the still-OPEN producer rows in `dsrrl.renderer_producer_map`: ClipPlane, DofParam, LocalWorldMtx, LocalWorldMtx_WithPrev, Normal2Alpha and Water. DSR-only DeferredLighting/Glow/WindParam remain a separate consumer-routing census before any decision to preserve/bypass them.


---

## 2026-09-23 — DSR comparison: depth transport, shadow PCF and velocity

Status: **CONFIRMED shader-bytecode delta / pending Supabase backfill**

Source: exact DSR `FRPG_FlverPBL_fpo_DX11.shaderbnd.dcx` extracted from the project evidence pack and parsed as BND3/DXBC.

### DSR depth producer no longer emits PTDE packed RGB depth

Exact DSR counterparts:
- index 1498 `FRPG_Phn_Dif______Mul_______DepAlp.fpo`, SHA-256 `5a0a6b872a697ad70bf3c35612eca1e1b2633bceb222b0b6f832ffed4bdfef6a`
- index 1613 `FRPG_Phn_Dif________________Dep.fpo`, same SHA-256
- index 1614 `FRPG_Phn_Dif________________DepAlp.fpo`, SHA-256 `6f4cb00e2320f553611120adae0af442ed017dbe32482e642bf6b945eb787eb5`

The first two payloads contain only `dcl_global_flags; ret`. They perform no color packing at all.

The DSR `DepAlp` shader performs only alpha-test logic and also emits no packed depth RGB. RDEF identifies `AlphaTestBuffer` containing `AlphaTest` and `AlphaTestRef`. Its effective pixel-side gate is:

`A = sampled_alpha * v2.w`

When `AlphaTest == 1`, pixels satisfying `A <= AlphaTestRef` are discarded. Exact upstream meaning of `v2.w` remains an upstream/VS routing question; do not label it `c100.w` without that RE.

Therefore the DSR shadow-depth producer path is structurally nonhomologous to PTDE's RGB-packed color-depth producer. DSR relies on raster/depth state rather than reconstructing the PTDE color payload in these pixel shaders.

### DSR FaceEye Sdw/Csd shadow consumer

Exact DSR FaceEye Sdw/Csd DXBC uses resource `t7/s7` with hardware comparison sampling:

`PCF_DSR = (1/9) * sum_{u=-1..1,v=-1..1} sample_c(t7,s7,shadowUV,receiverDepth,offset(u,v))`

The compiled offsets are exactly the 3x3 integer grid:
`(-1,-1),(0,-1),(1,-1),(-1,0),(0,0),(1,0),(-1,1),(0,1),(1,1)`.

This is not the PTDE 16-tap 4x4 manual packed-depth comparison kernel.

RDEF maps the relevant DSR constants:
- `cb0[21] = gFC_ShadowMapParam`
- `cb0[22] = gFC_ShadowColor`
- `cb0[56..59] = gFC_ShadowMapClamp`
- `cb0[73] = gFC_ShadowLightDir`
- `cb0[101] = gFC_DebugPointLightParams`

The DSR post-PCF response is:

`Bias = saturate((dot(gFC_ShadowLightDir.xyz,N)+gFC_ShadowMapParam.x)*gFC_ShadowMapParam.w)`

`Fade = saturate((gFC_ShadowMapParam.y-viewDistance)*gFC_ShadowMapParam.z)`

`S = min(1, PCF_DSR + Bias)`

`ShadowBase = 1 - S*Fade*gFC_ShadowColor.rgb`

Then DSR applies an additional component-wise power encoded by LOG/MUL/EXP:

`ShadowRGB_DSR = abs(ShadowBase) ^ gFC_DebugPointLightParams.z`.

For the expected non-negative shadow-color range, this reduces to `ShadowBase ^ gFC_DebugPointLightParams.z`.

The exact hardware comparison function bound to `s7` is state outside the shader bytecode and remains to be named explicitly; the shader proves comparison sampling and the 9-tap kernel, not by itself the sampler-state inequality.

### PTDE vs DSR shadow operator delta

PTDE FaceEye:
- depth producer: RGB-packed 24-bit-like representation in color output;
- consumer decode: manual `dot(RGB,[255/256,255/65536,255/16777216])`;
- PCF: 16 taps on a 4x4 grid with offsets `{-3,-1,+1,+3}/4096`;
- colored shadow response;
- no corresponding post-shadow `pow(ShadowRGB,k)` in the recovered FaceEye path.

DSR FaceEye:
- producer pixel shader does not pack color depth;
- consumer: hardware `sample_c`;
- PCF: 9 taps on a 3x3 integer-offset grid;
- same broad bias/fade/colored-shadow structure;
- additional `ShadowRGB ^ gFC_DebugPointLightParams.z` transform.

This is a **structural shadow-operator mismatch**, not a PARAM-only scalar mismatch and not an asset-only mismatch.

### Bridge consequence

A canonical PTDE shadow bridge should be treated as a shader/resource-state operator island. The narrowest plausible implementation is the exact routed Sdw/Csd receiver family plus the existing DSR depth resource, reproducing PTDE's comparison/kernel/colored response at the consumer cut. Exact equality still requires the DSR depth semantic and sampler comparison state to be closed; injecting PTDE packed RGB blindly into the DSR `sample_c` path would be wrong.

The PTDE pack/decode pair reconstructs stored shadow depth as:
`d_stored = (255.99998474121094/256)*d = 0.9999999403953552*d`.

If an exact bridge retains DSR hardware depth rather than PTDE packed color, this tiny compiled PTDE scaling belongs to the comparison semantics and must be handled explicitly if it is decision-relevant; it is not permission for an arbitrary bias.

### DSR velocity path

Exact DSR `FRPG_Phn_Dif________________Vel.fpo`, index 1634, SHA-256 `d5705d1b01f9b9dc3ca2e468642e99c526758c2a0f0b0e7d59b69a1965468e46`.

Input signature:
- `v1 = TEXCOORD0`
- `v2 = TEXCOORD1`

Pixel shader computes:

`CurrentUV = 0.5*(v1.xy/v1.w) + 0.5`
`PreviousUV = 0.5*(v2.xy/v2.w) + 0.5`
`o0.xy = CurrentUV - PreviousUV`
`o0.z = 0`
`o0.w = 1`.

So unlike PTDE `Vel`, which simply passes through an upstream velocity payload, DSR moves the clip-position-to-motion-vector conversion into the pixel shader.

DSR `VelAlp`, index 1635, SHA-256 `bcbb999aa5a9e9c5cf8da4560d53dd195ea44f04e1c5bbcb11a1731e9d4f592a`, performs the same velocity math after its AlphaTestBuffer gate and exports the surviving alpha in `o0.w`.

### Velocity bridge consequence

PTDE and DSR do not expose the same semantic input at the pixel-shader cut:
- PTDE PS consumes an already-produced velocity vector;
- DSR PS consumes current/previous clip-position varyings and derives velocity.

Therefore the correct comparison/bridge must move one stage upstream and reconstruct the PTDE velocity producer before choosing PS vs VS/transport as carrier. A pixel-shader-only PTDE port would otherwise reproduce the visible formula without proving equivalent source semantics.


---

## 2026-09-23 — PTDE PHN `Non` family census (48 shaders)

Status: **CONFIRMED PTDE family structure and core math / DSR delta partially closed / pending Supabase backfill**

Corpus: all 48 PTDE `FRPG_Phn_*_Non.fpo` shaders. The family is the full product of:
- Spc off/on,
- Bmp off/on,
- Mul off/on,
- Lit off/on,
- shadow route: none / Sdw / Csd.

There are 48 filenames but only **24 unique PTDE payloads**.

### Bmp is an exact PTDE pixel-shader no-op in `Non`

Every Bmp variant has a byte-identical non-Bmp counterpart:
**24/24 exact binary pairs**.

Examples include all combinations of Spc/Mul/Lit and all none/Sdw/Csd routes.

Therefore the PTDE `Non` pixel shader does not consume bump/normal-map math at all. A future normal bridge must not expand into this family merely because the filename contains `Bmp`.

DSR still has 48 distinct payload bytes for the same 48 names. Representative DSR Bmp/non-Bmp diffs show shifted input-register ABI while retaining the same pixel math in the inspected pairs; a complete DSR semantic-equivalence census is still OPEN, so only the PTDE 24/24 binary identity is canonical here.

### Base `Non` material cut

The family has no PTDE Upper/Lower, EnvDiffuse probe, EnvSpec probe or local PointLight term in the recovered pixel-shader core.

For ordinary non-Mul diffuse:
`D = (tex2D(s0,uv).rgb + c156.rgb) * c100.rgb * VertexColor.rgb`.

Alpha follows the ordinary diffuse/material route; exact route differs for Mul and is kept separate below.

For `Spc`:
`S = tex2D(s1,uv).rgb * c101.rgb * VertexColor.rgb`.

PTDE `Non` combines this directly:
`MaterialRGB = D + S`.

There is no directional/specular BRDF evaluation around this `S` term in the `Non` pixel shader. In this family the SpecRGB/Spc texture behaves as a direct additive material-color contribution before common fog/light-scattering.

This makes `Non` a separate material-response class from HemEnv/HemDir3/FaceEye.

### Mul route is texture blending, not a scalar multiply

For `Mul`, PTDE samples a second diffuse resource at `s3` and uses vertex-color alpha as the blend coordinate:

`D0 = tex2D(s0,uv0).rgb`
`D1 = tex2D(s3,uv1).rgb + c156.rgb`
`Dmix = lerp(D0,D1,VertexColor.a)`
`D = Dmix * c100.rgb * VertexColor.rgb`.

When `Spc+Mul` is present:
`S0 = tex2D(s1,uv0).rgb`
`S1 = tex2D(s4,uv1).rgb`
`Smix = lerp(S0,S1,VertexColor.a)`
`S = Smix * c101.rgb * VertexColor.rgb`.

So the `Mul` name must not be interpreted as a generic post-lighting multiplier. It selects a two-resource vertex-alpha blend subgraph.

### Lit route

`Lit` samples `s6`.

Without a shadow route:
`Gate = tex2D(s6,lightUV).rgb`.

The material cut becomes:
`SurfaceRGB = (D + S) * Gate`
with absent `S` treated as zero.

### Sdw/Csd route

The `Non` family reuses the same PTDE packed-depth 16-tap 4x4 shadow kernel recovered for FaceEye:
- packed RGB depth decode;
- 16 manual comparisons;
- bias from `c175/c121`;
- distance fade from `c121`;
- colored `ShadowRGB = 1 - ShadowAmount*ShadowFade*c122.rgb`.

For shadowed `Non` without `Lit`:
`Gate = ShadowRGB`.

For `Lit + Sdw/Csd`, the composition is not multiplicative:
`Gate = min(LightMap.rgb, ShadowRGB)`
component-wise.

Then:
`SurfaceRGB = (D + S) * Gate`.

This `min` composition is an operator-level behavior and must be preserved if `Non` is ported. Replacing it by `LightMap*Shadow` changes the PTDE response.

### Common downstream

After the `Non` material cut, the family enters the same legacy fog/light-scattering style tail and terminal PTDE scene encoding already identified elsewhere. The bridge-relevant narrow cut is the material/gate output before that shared downstream.

### Initial DSR delta from exact counterparts

All 48 DSR `*_Non` counterparts exist in the audited binder (indices 737..1633).

Representative DSR disassembly shows:
- explicit AlphaTestBuffer gate;
- gamma/domain transforms around diffuse/spec/lightmap/shadow terms;
- DSR Sdw/Csd uses the 9-tap 3x3 hardware `sample_c` shadow operator documented above;
- shadow RGB is additionally transformed by `gFC_DebugPointLightParams.z`;
- Spc is no longer simply added in the PTDE legacy domain: DSR wraps material terms through explicit `pow(2.2)` / `pow(1/2.2)`-equivalent LOG/MUL/EXP sequences;
- Lit similarly raises the lightmap by the DSR exponent path before composition.

This establishes a real PTDE↔DSR material/domain mismatch in `Non`; it is not reducible to restoring one texture or one global gain.

### Bridge consequence

A future `Non` bridge should be treated as its own receiver family with exact shader identity:
1. preserve PTDE direct additive Spc behavior;
2. preserve `Mul` as the two-resource vertex-alpha blend graph;
3. preserve Lit gate semantics;
4. preserve `min(LightMap,ShadowRGB)` for Lit+shadow;
5. keep Bmp inactive in the PTDE pixel-material operator;
6. keep common fog/light-scattering downstream independent;
7. fail-open for non-`Non` receivers.

The likely narrowest carrier is a shader/material-response island, with shadow resource-state work only for Sdw/Csd variants. Global LightBank/PARAM compensation would target the wrong operator.


### DSR `Non` representative material-domain delta — supplemental

Representative DSR `Non` DXBC disassembly sharpens the family mismatch beyond the shadow kernel.

Relevant RDEF mappings observed in the simple `Non` host include:
- `cb0[9] = gFC_DifMapMulCol`
- `cb0[10] = gFC_SpcMapMulCol`
- `cb0[12] = gFC_FogCol`
- `cb0[13] = gFC_LsBeta1PlusBeta2`
- `cb0[14] = gFC_LsTerrainReflectance`
- `cb0[15] = gFC_LsOneOverBeta1PlusBeta2`
- `cb0[16] = gFC_LsHGg`
- `cb0[17] = gFC_LsBetaDash1`
- `cb0[18] = gFC_LsBetaDash2`
- `cb0[19] = gFC_LsSunColor`
- `cb0[20] = gFC_LsLightDir`
- `cb0[60] = gFC_FgSkinAddColor`
- `cb0[101] = gFC_DebugPointLightParams`.

Observed DSR material-domain structure:
- simple diffuse product still samples diffuse, adds `gFC_FgSkinAddColor`, and multiplies `gFC_DifMapMulCol * VertexColor`;
- DSR then uses explicit LOG/MUL/EXP exponent transforms instead of keeping the PTDE legacy-domain material product;
- in `Spc`, diffuse is first mapped through the 2.2-domain path, the spec material contribution is added in that domain, then the result is mapped through the reciprocal exponent before common fog/scattering and later re-encoded;
- in `Lit`, the diffuse material enters the 2.2-domain path and the lightmap sample is independently transformed by the exponent carried through `gFC_DebugPointLightParams.z` before multiplication;
- in `Sdw/Csd`, DSR uses the previously documented 9-tap comparison-sampler shadow plus the additional shadow-color exponent before material composition.

Therefore PTDE `Non` direct `(D+S)*Gate` and DSR `Non` are mathematically different material operators even when the same texture assets are bound. This strengthens the bridge classification to **shader/material-response**, with shadow state as a separate sub-operator for Sdw/Csd.

PTDE Bmp/non-Bmp remains **24/24 byte-identical**. DSR Bmp/non-Bmp payloads are byte-different because the inspected pairs use different input-varying layouts; representative arithmetic is otherwise equivalent, but a complete semantic-normalized 24/24 DSR proof remains OPEN. Do not promote “Bmp is globally a DSR no-op” without that census.
