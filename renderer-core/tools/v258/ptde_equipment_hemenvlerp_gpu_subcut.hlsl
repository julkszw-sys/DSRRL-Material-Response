// V258 standalone PTDE DifSpcBmp HemEnvLerp GPU operator subcut.
// Compiled with d3dcompiler_47 into DXBC; never auto-bound to a live DSR PS.
// Exact PTDE c84.w and c85.w are separate. Filtered RGBA divide occurs
// BEFORE the A/B interpolation. No t9 BRDF LUT, no DSR roughness/LOD.
// This shader ends BEFORE c139 / Fog / LightScattering / c110 terminal;
// output is an intermediate, NOT PTDE visible COLOROUT0.
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
    const float3 diff_a=(da.rgb/da.a)*ptde_c86.rgb;
    const float3 diff_b=(db.rgb/db.a)*ptde_c84.rgb;
    const float3 spec_a=(sa.rgb/sa.a)*ptde_c87.rgb;
    const float3 spec_b=(sb.rgb/sb.a)*ptde_c85.rgb;
    const float3 diff=diff_a+ptde_c84.w*(diff_b-diff_a);
    const float3 spec=spec_a+ptde_c85.w*(spec_b-spec_a);
    const float3 ul=ptde_c99.rgb+input.hemisphereT*(ptde_c98.rgb-ptde_c99.rgb);
    const float3 dtex=ptdeDiffuseTex.Sample(ptdeSampler,input.uv).rgb;
    const float3 stex=ptdeSpecTex.Sample(ptdeSampler,input.uv).rgb;
    const float3 diffuse=(diff+ul)*(dtex+ptde_c156.rgb)*ptde_c100.rgb*input.vertexColor;
    const float3 reflection=spec*stex*ptde_c101.rgb*input.vertexColor;
    return float4(diffuse+reflection,1);
}
