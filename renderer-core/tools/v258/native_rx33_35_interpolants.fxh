// V267: semantic RX33/34/35 input layout reconstructed from the DSR retail
// FRPG_Common_VTX_OUT.fxh + exact ISGN of original stock DXBC.
// Native DSR source names apply, but are NOT an authorization to supply PTDE
// EnvDiffuseDir, ReflectionDir, Upper/Lower or Fog inputs from these fields.
// Receiver-specific helpers may consume this layout only after exact PS SHA,
// FLVER/MTD/slot and PTDE material/resource producer proof.
#ifndef DSRRL_V267_RX_NATIVE_INPUTS_INCLUDED
#define DSRRL_V267_RX_NATIVE_INPUTS_INCLUDED
struct DSRRL_NativeRX33_CsdInput {
    float4 VtxClp : SV_Position;
    float4 VtxWld : TEXCOORD0;   // world xyz; view z in w
    float4 VecNrm : TEXCOORD2;   // normal xyz; native Fog factor in w
    float4 VecEye : TEXCOORD3;   // eye vector xyz, distance in w
    float4 VecTan : TEXCOORD4;
    float3 VecTan2 : TEXCOORD5;  // exact stock ISGN mask xyz
    float4 ColVtx : COLOR0;
    float2 TexDifDif : TEXCOORD6; // exact stock ISGN mask xy
    bool IsFrontFace : SV_IsFrontFace;
};
struct DSRRL_NativeRX34_SdwInput {
    float4 VtxClp : SV_Position;
    float4 VtxWld : TEXCOORD0;
    float4 VtxLit : TEXCOORD1;  // shadow-light position; RX34 ONLY
    float4 VecNrm : TEXCOORD2;
    float4 VecEye : TEXCOORD3;
    float4 VecTan : TEXCOORD4;
    float3 VecTan2 : TEXCOORD5;
    float4 ColVtx : COLOR0;
    float2 TexDifDif : TEXCOORD6;
    bool IsFrontFace : SV_IsFrontFace;
};
struct DSRRL_NativeRX35_PlainInput {
    float4 VtxClp : SV_Position;
    float4 VtxWld : TEXCOORD0;
    float4 VecNrm : TEXCOORD2;
    float4 VecEye : TEXCOORD3;
    float4 VecTan : TEXCOORD4;
    float3 VecTan2 : TEXCOORD5;
    float4 ColVtx : COLOR0;
    float2 TexDifDif : TEXCOORD6;
    bool IsFrontFace : SV_IsFrontFace;
};

// These are native OUTPUT ABI types only. Actual production host RT1 is
// written by stock DSR renderer and must be preserved byte-for-byte.
// Original common HLSL can use SV_Target1 for either Glow (PS_OUT_SFX) or
// GBuffer auxiliary/Subsurface (GBUFFER_OUT). Do NOT synthesize RT1 here.
struct DSRRL_NativeDualTargetSignature {
    float4 stockColor : SV_Target0;
    float4 stockAuxiliary : SV_Target1;
};
#endif
