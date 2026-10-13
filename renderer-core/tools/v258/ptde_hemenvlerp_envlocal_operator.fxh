// V267: extracted PTDE HemEnvLerp EnvDiffuse/EnvSpec + local equipment surface.
// This is an INLINE OPERATOR, never a standalone DSR pixel-shader replacement.
// GPU sampling (and its filtering/LOD), bound CB values and receiver interpolants
// must be authenticated separately. In particular, never synthesize native
// Glow/Subsurface SV_Target1 or replace stock DSR Fog/LightScattering.
//
// Original PTDE DifSpcBmp HemEnvLerp ps_3_0:
//  0511..0542 raw filtered RGBA -> divide by filtered alpha -> EnvDiffuse
//  0611..0642 same for EnvSpec
//  0651..0679 local material composition SpecTex RGB (not sampled alpha).
#ifndef DSRRL_V267_PTDE_HEMENVLERP_ENVLOCAL_INCLUDED
#define DSRRL_V267_PTDE_HEMENVLERP_ENVLOCAL_INCLUDED

struct DSRRL_PTDE_EnvLocal {
    float4 envDiffuseA_RGBA;
    float4 envDiffuseB_RGBA;
    float4 envSpecA_RGBA;
    float4 envSpecB_RGBA;
    float3 c84_rgb, c86_rgb;
    float3 c85_rgb, c87_rgb;
    float c84_w, c85_w;
    float3 upperLower;
    float3 diffuseTex_RGB, specTex_RGB;
    float3 c100_rgb, c101_rgb, c156_rgb, vertex_RGB;
};

float3 DSRRL_PTDE_EnvLocalRGB(DSRRL_PTDE_EnvLocal s) {
    // Order matters: divide the FILTERED RGBA, not alpha-correct each texel.
    const float3 da = (s.envDiffuseA_RGBA.rgb / s.envDiffuseA_RGBA.a) * s.c86_rgb;
    const float3 db = (s.envDiffuseB_RGBA.rgb / s.envDiffuseB_RGBA.a) * s.c84_rgb;
    const float3 sa = (s.envSpecA_RGBA.rgb / s.envSpecA_RGBA.a) * s.c87_rgb;
    const float3 sb = (s.envSpecB_RGBA.rgb / s.envSpecB_RGBA.a) * s.c85_rgb;

    // PTDE uses independent c84.w / c85.w; these are not c110.w.
    const float3 envDiffuse = da + s.c84_w * (db - da);
    const float3 envSpec = sa + s.c85_w * (sb - sa);

    const float3 diffuse = (envDiffuse + s.upperLower) *
        (s.diffuseTex_RGB + s.c156_rgb) * s.c100_rgb * s.vertex_RGB;
    const float3 specular = envSpec * s.specTex_RGB * s.c101_rgb * s.vertex_RGB;
    return diffuse + specular;
}
#endif // DSRRL_V267_PTDE_HEMENVLERP_ENVLOCAL_INCLUDED
