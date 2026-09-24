# PTDE legacy shader-math RE — PHN FaceEye no-shadow family

Status: RESEARCH COMPLETE FOR NO-SHADOW CORE / pending Supabase sync
Date: 2026-09-23
Scope: PTDE FRPG_Phn_FaceEye____ variants with 0/1/2/4 PointLights.

## Provenance
Source evidence pack: DSRRL_FINAL_EVIDENCE_PART02(2).zip
Members: PTDE/PTDE_FRPG_PHN_EXTRACTED/dvdbnd1_dcx_0000631D9390/

SHA-256:
- FaceEye____: fcba0b90ad711340f52045156aa8b17f9d3ba7a175e5815138399f997389164a
- FaceEye____PntS: 3feaf1b3881902b705d3a6321e3994ce03676995c040e450baee74357e0f5f04
- FaceEye____PntSS: d7c100456d2d06bd92832cd831a8cd6fc65c50e586c788b9e4e5d0e420cc4ed1
- FaceEye____PntSSSS: 3574828ba1e498f981ec2e908f388f86ced625e47dae960141a0af748185e085

## Exact PTDE core
N=normalize(normal); V=normalize(view); R=2*dot(N,V)*N-V; t=0.5*N.y+0.5.
EnvDifProbe=sampleCube(s11,N).rgb/sampleCube(s11,N).a.
EnvSpcProbe=sampleCube(s12,R).rgb/sampleCube(s12,R).a.
H=c99.rgb+t*(c98.rgb-c99.rgb).
EnvDiffuse=H+c86.rgb*EnvDifProbe.
EnvSpec=c87.rgb*EnvSpcProbe.

Td=sample2D(s0,uv).
FaceBase=Td.rgb*(1+Td.a*(c174.rgb-1))+c156.rgb.
DiffuseMaterial=FaceBase*c100.rgb*VertexColor.rgb.
Ts=sample2D(s1,uv).
SpecMaterial=Ts.rgb*c101.rgb*VertexColor.rgb.
SurfaceRGB=DiffuseMaterial*EnvDiffuse+SpecMaterial*EnvSpec.
SurfaceA=c100.a*VertexColor.a.

## PointLight
For each light i:
D_i=P_i-P_surface; d_i=length(D_i); L_i=D_i/d_i.
A_i=saturate((End_i-d_i)*InvSpan_i).
Light_i=A_i*Color_i.
PointDiffuse_i=max(dot(N,L_i),0)*Light_i.
PointSpec_i=pow(max(dot(R,L_i),0),c102.x)*Light_i.
DiffuseLighting=EnvDiffuse+sum(PointDiffuse_i).
SpecLighting=EnvSpec+sum(PointSpec_i).
SurfaceRGB=DiffuseMaterial*DiffuseLighting+SpecMaterial*SpecLighting.

PTDE attenuation is linear for PntS/PntSS/PntSSSS. Existing DSR census classifies FaceEye PntS as cubic-special and PntSS/PntSSSS as linear-special. Do not blanket patch the family: narrow future PntS candidate is shader-local x^3 -> x; BRDF/material response remains a separate coordinate.

## Terminal
After common PHN fog/scattering: oC0.rgb=saturate(PostFogRGB*c135.x/c135.y).

## Status
CONFIRMED: no-shadow core math, PointLight attenuation, local Phong specular, RGB/A legacy probe decode, narrow pre-fog material cut.
OPEN: authored semantic names for c156/c174; DSR FaceEye full BRDF/material equation; Sdw/Csd shadow subgraphs.
