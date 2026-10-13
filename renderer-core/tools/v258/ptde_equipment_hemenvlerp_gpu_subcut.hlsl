// V267: V258 standalone GPU harness now calls reusable inline PTDE operator.
// Compiled with d3dcompiler_47 into DXBC; never auto-bound to a live DSR PS.
// Exact PTDE c84.w and c85.w are separate. Filtered RGBA divide occurs
// BEFORE the A/B interpolation. No t9 BRDF LUT, no DSR roughness/LOD.
// This shader ends BEFORE c139 / Fog / LightScattering / c110 terminal;
// output is an intermediate, NOT PTDE visible COLOROUT0.
#include "ptde_hemenvlerp_envlocal_operator.fxh"

cbuffer V258_PTDE_HEMENVLERP_CB : register(b12) {
    float4 ptde_c84;
    float4 ptde_c85;
    float4 ptde_c86;
    float4 ptde_c87;
    float4 ptde_c98;
    float4 ptde_c99;
    float4 ptde_c100;
    float4 ptde_c101;
    float4 ptde_c156;
};
Texture2D<float4> ptdeDiffuseTex : register(t0);
Texture2D<float4> ptdeSpecTex : register(t10);
TextureCube<float4> ptdeEnvDiffuseA : register(t11);
TextureCube<float4> ptdeEnvDiffuseB : register(t13);
TextureCube<float4> ptdeEnvSpecA : register(t12);
TextureCube<float4> ptdeEnvSpecB : register(t14);
SamplerState ptdeSampler : register(s0);

struct Input {
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
    float3 envDiffuseDir : TEXCOORD1;
    float3 reflectionDir : TEXCOORD2;
    float3 vertexColor : COLOR0;
    float hemisphereT : TEXCOORD3; // must come from verified PTDE normal kernel
};
// Input vectors/hemisphereT are PRECOMPUTED *references*. No assumption
// that stock DSR PS input signature supplies these semantic values yet.
float4 main(Input input) : SV_Target0 {
    const float4 da=ptdeEnvDiffuseA.Sample(ptdeSampler,input.envDiffuseDir);
    const float4 db=ptdeEnvDiffuseB.Sample(ptdeSampler,input.envDiffuseDir);
    const float4 sa=ptdeEnvSpecA.Sample(ptdeSampler,input.reflectionDir);
    const float4 sb=ptdeEnvSpecB.Sample(ptdeSampler,input.reflectionDir);
    DSRRL_PTDE_EnvLocal op;
    op.envDiffuseA_RGBA = da;
    op.envDiffuseB_RGBA = db;
    op.envSpecA_RGBA = sa;
    op.envSpecB_RGBA = sb;
    op.c84_rgb = ptde_c84.rgb;
    op.c86_rgb = ptde_c86.rgb;
    op.c85_rgb = ptde_c85.rgb;
    op.c87_rgb = ptde_c87.rgb;
    op.c84_w = ptde_c84.w;
    op.c85_w = ptde_c85.w;
    op.upperLower = ptde_c99.rgb + input.hemisphereT *
        (ptde_c98.rgb - ptde_c99.rgb);
    op.diffuseTex_RGB = ptdeDiffuseTex.Sample(ptdeSampler,input.uv).rgb;
    op.specTex_RGB = ptdeSpecTex.Sample(ptdeSampler,input.uv).rgb;
    op.c100_rgb = ptde_c100.rgb;
    op.c101_rgb = ptde_c101.rgb;
    op.c156_rgb = ptde_c156.rgb;
    op.vertex_RGB = input.vertexColor;
    return float4(DSRRL_PTDE_EnvLocalRGB(op), 1);
}
