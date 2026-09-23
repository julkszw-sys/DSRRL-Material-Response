# FaceEye static closure gate — 2026-09-23

Status: **pending Supabase sync**. The required `dsrrl.agent_bootstrap('PTDE legacy shader math RE')` + `dsrrl.renderer_addon_control_plane_status(null)` call was attempted first and blocked by the tool safety layer. No partial Supabase mutation was attempted.

## Scope
Receiver-first continuation of PTDE `FRPG_Phn_FaceEye_*` shader-math RE. This run re-audits available persistent artifacts to determine what can and cannot be promoted without inventing missing DSR DXBC algebra.

## Persistent evidence revalidated
The P1.6 diagnostic runtime log states that the validated P1.5 baseline added terminal RGB SAT to exactly **12 FaceEye retained PTDE homologs**. The 60 added Non/FaceEye hashes were singleton targets with no broad alias collateral; `PHN_EXTRA_SAT=60/60`. This remains identity/receiver-boundary evidence, not BRDF-equation evidence.

The same P1.6 plan had the previously established `PNTS_LINEAR=72/72` general PHN PointLight attenuation bridge active, but the log does not enumerate a FaceEye-specific PntS attenuation patch. Therefore it cannot by itself prove the exact DSR FaceEye PntS instruction sequence.

## PTDE side already closed
Prior static PTDE FaceEye RE establishes for each local light:

```
D = P_light - P_surface
d = length(D)
L = D / d
A = saturate((End - d) * InvSpan)
Light = A * Color
PointDiffuse = max(dot(N,L),0) * Light
PointSpec = pow(max(dot(R,L),0), SpecPower) * Light
```

and the FaceEye base-material law:

```
FaceBase = Td.rgb * (1 + Td.a * (c174.rgb - 1)) + c156.rgb
DiffuseMaterial = FaceBase * c100.rgb * VertexColor.rgb
SpecMaterial = Ts.rgb * c101.rgb * VertexColor.rgb
```

No explicit gamma/pow transform occurs inside this local FaceEye material/light operator. Terminal scene encoding and Fog/LightScattering remain downstream and outside the narrow operator cut.

## New classification from this run
The exact 12-receiver DSR FaceEye island remains **CONFIRMED**. The proposed `FaceEye PntS cubic -> linear` carrier remains **HIGH CONFIDENCE only**, not CONFIRMED, until static DSR consumer bytecode is available and decoded. The evidence currently accessible in the project Library is diagnostic runtime identity/coverage logging rather than the raw DSR FaceEye DXBC bodies; promoting a full DSR FaceEye equation from those logs would violate receiver-first/static-evidence rules.

This also means a FaceEye material/specular bridge remains **OPEN**. The PTDE equation is known, but no DSR mismatch operator may be asserted until the DSR homolog equation is recovered.

## Narrow future bridge unlocked / gated
If static DSR bytecode confirms the existing census classification, the narrow attenuation carrier is:

```
receiver set = exact FaceEye PntS homolog(s) only
operator delta = DSR cubic attenuation -> PTDE linear A
PntSS/PntSSSS = fail open
shadow/environment = untouched
terminal SAT = separate operator
material/specular = untouched until independently proven
```

## Missing artifact needed for the next static step
Raw/disassembled DSR DXBC for shader IDs `358,646,173,1068,442,1226,549,560,928,641,1077,133` (the 12 active FaceEye host counterparts), or a persistent artifact containing those shader bodies. No new runtime capture is required; this is an artifact-availability gap only.

## Supabase sync plan
When Supabase calls are accepted again: submit this audit as raw evidence, avoid duplicate canonicals, retain receiver boundary CONFIRMED / attenuation carrier HIGH CONFIDENCE / material bridge OPEN, then update `shader_math_*` only with fields supported by static evidence.