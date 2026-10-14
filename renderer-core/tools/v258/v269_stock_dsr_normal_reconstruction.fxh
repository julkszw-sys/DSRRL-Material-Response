// V269: isolated normal-map source from owner original DSR DX11 Common
// FRPG shader HLSL: DecodeNormalMap, CalcGetNormal_FromNormalTex_Mul_Bin.
// Scope: ONE certified DSR t2 normal resource, a single-map receiver and
// valid normal strength. Two-normal blended FLVER routes, detail bump,
// parallax and PTDE normal-map homology remain OUT OF SCOPE.
//
// This file is an INLINE GPU helper, not a PS replacement or a permission
// to bind PTDE SpecRGB against a surviving stock DSR PBL tail.
#ifndef DSRRL_V269_STOCK_DSR_ONE_MAP_NORMAL_INCLUDED
#define DSRRL_V269_STOCK_DSR_ONE_MAP_NORMAL_INCLUDED

float3 DSRRL_V269_DecodeDSRNormalRGBA(float4 colTex) {
    // FROM original DSR Common source; DXT1 vs DXT5 compatible:
    // (x,y) = (r*a,g*b)*2-1; z = sqrt(1-saturate(x*x+y*y))
    const float2 xy = colTex.rg * colTex.ab * 2.0f - 1.0f;
    return float3(xy, sqrt(1.0f - saturate(dot(xy,xy))));
}

float3 DSRRL_V269_SingleMapDSRNormal(
    float4 sampledRGBA,float3 vecNrm,float4 vecTan,float normalScale) {
    const float3 n = normalize(vecNrm);
    const float3 t = normalize(vecTan.xyz);
    const float3 b = normalize(cross(vecNrm,vecTan.xyz) * vecTan.w);
    const float3 v = lerp(float3(0,0,1),
                            DSRRL_V269_DecodeDSRNormalRGBA(sampledRGBA),
                            normalScale);
    // The exact original Common single-map binormal/tangent convention.
    return normalize(b*v.x + t*v.y + n*v.z);
}
#endif
