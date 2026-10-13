// Exact PTDE ps_3_0 0782-0794 semantic cut, without unproven
// attribution of the upstream atmospheric producers in stock DSR.
// r0=pre-LRP r0, r1=pre-LRP r1, w=c110.w.
// Native PTDE ps_3_0 lrp r2.xyz,c110.wwww,r1,r0; scale c135.x/y.
cbuffer V258_PTDE_TERMINAL_CB : register(b12) {
    float4 ptde_c110;
    float4 ptde_c135;
};
struct Input {
    float4 position : SV_Position;
    float3 pre_lrp_r0 : TEXCOORD0;
    float3 pre_lrp_r1 : TEXCOORD1;
};
float4 main(Input input) : SV_Target0 {
    const float3 mixed=input.pre_lrp_r0+
        ptde_c110.w*(input.pre_lrp_r1-input.pre_lrp_r0);
    const float3 out_rgb=mixed*(ptde_c135.x/ptde_c135.y);
    return float4(out_rgb,1);
}
