# FaceEye DSR homologue boundary — 2026-09-23

Status: **pending Supabase sync**. Supabase bootstrap/control-plane call was attempted first but blocked by the tool safety layer; no partial database mutation was attempted.

## Scope
Receiver-first follow-up to the PTDE `FRPG_Phn_FaceEye_*` static RE. This note records only newly established DSR-side boundary evidence; it does not widen the PTDE formulas beyond the already-censused FaceEye family.

## New evidence
An existing DSR runtime diagnostic log for `DSRRL P1.6 Non/FaceEye SAT Expansion` explicitly reports that the diagnostic preserves the validated P1.5 baseline and adds terminal RGB SAT to **12 FaceEye retained PTDE homologs**, with the 60 added Non/FaceEye hashes singleton (`broad=0`, no alias collateral). The same run reports `PHN_EXTRA_SAT=60/60` and terminal SAT coverage active while HemDir3/Bloom/HDR remain off.

This is DSR receiver-identity / patch-boundary evidence, not proof of the full DSR FaceEye BRDF equation. It establishes that the 12 FaceEye DSR receivers were already isolated as exact retained homolog targets for terminal RGB SAT and therefore provide a narrow receiver set for future FaceEye-specific operator work.

## Consequence for bridge design
The PTDE FaceEye RE already establishes linear PointLight attenuation for PntS/PntSS/PntSSSS and a FaceEye-specific base-material response. Existing DSR census classifies FaceEye PntS as cubic-special while PntSS/PntSSSS are linear-special. Combined with the exact 12-receiver DSR homologue boundary, the narrow future attenuation carrier remains:

`FaceEye PntS only: DSR cubic attenuation -> PTDE linear attenuation`

Fail open for PntSS/PntSSSS and for any shader outside the exact FaceEye receiver set. Do not couple this attenuation bridge to terminal SAT, shadow/environment routing, or FaceEye material response.

## What is still OPEN
1. Full static DXBC algebra of the 12 DSR FaceEye receivers, especially local specular/material response.
2. Exact PTDE `Sdw` versus `Csd` shadow lookup/PCF producer algebra and sampler bindings.
3. Whether DSR FaceEye material response differs from PTDE `Td.rgb*(1 + Td.a*(c174.rgb-1)) + c156.rgb`; no bridge is justified until the DSR consumer equation is statically recovered.

## Classification
- Exact DSR FaceEye receiver boundary for retained PTDE terminal-SAT homologs: **CONFIRMED by existing runtime diagnostic identity log**.
- `FaceEye PntS cubic -> linear` future carrier: **HIGH CONFIDENCE**, pending static DSR consumer-equation closure before implementation.
- Any FaceEye material/specular bridge: **OPEN**.

No runtime capture is requested or required for the next step.