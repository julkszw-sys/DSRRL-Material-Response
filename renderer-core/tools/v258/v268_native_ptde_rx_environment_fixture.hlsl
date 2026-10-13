// V269: host DSR t2 normalmap-aware RX33/34/35 GPU fixture bridges
// reconstructed PTDE environment/local-material kernel.
//
// COMPILER FIXTURE ONLY, never a drop-in stock PS: RT1 is synthetic and
// stock DSR Fog/LightScattering are NOT computed here. The cubemap direction
// uses original DSR CalcEnvIBL() algebra; equivalence to PTDE is OPEN, as are
// the live producers of PTDE c84.w/c85.w, Upper/Lower, and texture owners.
//
// This verifies a real GPU shader can sample PTDE slot resources using
// native RX inputs. It makes no authorization or runtime/pixel claim.
#include "native_rx33_35_interpolants.fxh"
#include "ptde_hemenvlerp_envlocal_operator.fxh"
#include "v269_stock_dsr_normal_reconstruction.fxh"

cbuffer V268_PTDE_EnvLocal : register(b12) {
    float4 ptde_c84;
    float4 ptde_c85;
    float4 ptde_c86;
    float4 ptde_c87;
    float4 ptde_c100;
    float4 ptde_c101;
    float4 ptde_c156;
    float4 explicitly_unverified_ptde_upper_lower;
};
Texture2D<float4> ptdeDiffuseTex : register(t0);
Texture2D<float4> ptdeSpecTex : register(t10);
// Original DSR native gSMP_2: single-map normal RX only. Other material
// variants must not be authorized by this test fixture.
Texture2D<float4> stockNormalTex : register(t2);
TextureCube<float4> ptdeEnvDiffuseA : register(t11);
TextureCube<float4> ptdeEnvSpecA : register(t12);
TextureCube<float4> ptdeEnvDiffuseB : register(t13);
TextureCube<float4> ptdeEnvSpecB : register(t14);
SamplerState ptdeSampler : register(s0);
SamplerState stockNormalSampler : register(s2);
// This producer is UNKNOWN. Not a live DSR b13 writer; fixture-only.
cbuffer V269_UNVERIFIED_StockNormalScale : register(b13) {
    float4 host_normal_scale;
};

float3 V268_UnverifiedHostDirectionsColor(float2 uv, float3 vertexColor,
                                         float3 vertexNormal, float4 vertexTangent,
                                         float3 eyeVector, bool isFrontFace)
{
    // Original DSR: flip native vertex normal for backfaces, decode t2
    // normal map as (r*a,g*b)*2-1, transform tangent -> world. This remains
    // host DSR single-normal source; PTDE normal/sidecar parity is OPEN.
    const float3 nativeN = isFrontFace ? vertexNormal : -vertexNormal;
    const float4 sampledN = stockNormalTex.Sample(stockNormalSampler,uv);
    const float3 n = DSRRL_V269_SingleMapDSRNormal(
        sampledN,nativeN,vertexTangent,host_normal_scale.x);
    const float3 v = normalize(eyeVector);
    // DSR CalcEnvIBL uses material-normal, *not* vertex-normal, for R.
    const float3 r = 2.0f*dot(n,v)*n-v;

    DSRRL_PTDE_EnvLocal p;
    p.envDiffuseA_RGBA=ptdeEnvDiffuseA.Sample(ptdeSampler,n);
    p.envDiffuseB_RGBA=ptdeEnvDiffuseB.Sample(ptdeSampler,n);
    p.envSpecA_RGBA=ptdeEnvSpecA.Sample(ptdeSampler,r);
    p.envSpecB_RGBA=ptdeEnvSpecB.Sample(ptdeSampler,r);
    p.c84_rgb=ptde_c84.rgb; p.c86_rgb=ptde_c86.rgb;
    p.c85_rgb=ptde_c85.rgb; p.c87_rgb=ptde_c87.rgb;
    p.c84_w=ptde_c84.w; p.c85_w=ptde_c85.w;
    // No candidate from a static b12 float3 is a verified PTDE UL producer.
    p.upperLower=explicitly_unverified_ptde_upper_lower.rgb;
    p.diffuseTex_RGB=ptdeDiffuseTex.Sample(ptdeSampler,uv).rgb;
    p.specTex_RGB=ptdeSpecTex.Sample(ptdeSampler,uv).rgb;
    p.c100_rgb=ptde_c100.rgb;
    p.c101_rgb=ptde_c101.rgb;
    p.c156_rgb=ptde_c156.rgb;
    p.vertex_RGB=vertexColor;
    return DSRRL_PTDE_EnvLocalRGB(p);
}

// Auxiliary output deliberately depends on all host input signature fields;
// it is NOT real DSR Glow/GBUFFER data and MUST NEVER be used at runtime.
float4 V268_FixtureAux(float4 pos, float4 world, float4 nrm,
                       float4 eye, float4 tangent, float3 binormal,
                       float3 vertex, float2 uv, bool front, float4 shad) {
    float3 a=pos.xyz+world.xyz+eye.xyz+binormal+vertex+shad.xyz;
    float3 b=nrm.xyz+tangent.xyz+float3(uv,nrm.w);
    return float4(a-b, world.w+eye.w+tangent.w+shad.w+(front?1.f:2.f));
}
DSRRL_NativeDualTargetSignature rx33_v268(DSRRL_NativeRX33_CsdInput i) {
    DSRRL_NativeDualTargetSignature outv;
    outv.stockColor=float4(V268_UnverifiedHostDirectionsColor(
        i.TexDifDif,i.ColVtx.rgb,i.VecNrm.xyz,i.VecTan,i.VecEye.xyz,i.IsFrontFace),i.ColVtx.w);
    outv.stockAuxiliary=V268_FixtureAux(i.VtxClp,i.VtxWld,
        i.VecNrm,i.VecEye,i.VecTan,i.VecTan2,i.ColVtx.rgb,
        i.TexDifDif,i.IsFrontFace,float4(0,0,0,0));
    return outv;
}
DSRRL_NativeDualTargetSignature rx34_v268(DSRRL_NativeRX34_SdwInput i) {
    DSRRL_NativeDualTargetSignature outv;
    outv.stockColor=float4(V268_UnverifiedHostDirectionsColor(
        i.TexDifDif,i.ColVtx.rgb,i.VecNrm.xyz,i.VecTan,i.VecEye.xyz,i.IsFrontFace),i.ColVtx.w);
    outv.stockAuxiliary=V268_FixtureAux(i.VtxClp,i.VtxWld,
        i.VecNrm,i.VecEye,i.VecTan,i.VecTan2,i.ColVtx.rgb,
        i.TexDifDif,i.IsFrontFace,i.VtxLit);
    return outv;
}
DSRRL_NativeDualTargetSignature rx35_v268(DSRRL_NativeRX35_PlainInput i) {
    DSRRL_NativeDualTargetSignature outv;
    outv.stockColor=float4(V268_UnverifiedHostDirectionsColor(
        i.TexDifDif,i.ColVtx.rgb,i.VecNrm.xyz,i.VecTan,i.VecEye.xyz,i.IsFrontFace),i.ColVtx.w);
    outv.stockAuxiliary=V268_FixtureAux(i.VtxClp,i.VtxWld,
        i.VecNrm,i.VecEye,i.VecTan,i.VecTan2,i.ColVtx.rgb,
        i.TexDifDif,i.IsFrontFace,float4(0,0,0,0));
    return outv;
}
