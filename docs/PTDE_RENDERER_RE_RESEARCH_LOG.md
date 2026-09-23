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
