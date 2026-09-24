#pragma once

#include <cstddef>
#include <cstdint>

namespace dsrrl::operators::material_response::generated {

struct route_seed {
    const char *binding_key;
    const char *mtd_name;
    std::uint32_t route_index;
    float c101;
    std::uint8_t lod_min;
    std::uint8_t lod_max;
    const char *material_family;
    std::uint32_t receiver0;
    std::uint32_t receiver1;
    std::uint32_t receiver2;
    const char *sha256;
    bool known_hash_name_collision;
};

inline constexpr route_seed k_material_routes_v1[] = {
    {"mr.full24.mtd.p_leather_dsb_alp","P_Leather[DSB]_Alp.mtd",0u,1.500000f,0u,7u,"DifSpcBmp",33u,34u,35u,"4c728a9b5957a75d0eb82b2b77b800829e1973632c7c0690a7e31a074c85e7fb",false},
    {"mr.full24.mtd.p_leather_dsb_edge","P_Leather[DSB]_Edge.mtd",1u,1.500000f,0u,7u,"DifSpcBmp",33u,34u,35u,"cf968a22722667777d10429a03a2e4a87f867412949a7fe5590cb8cefaa2bfa4",false},
    {"mr.full24.mtd.p_metal_dsb_alp","P_Metal[DSB]_Alp.mtd",2u,2.500000f,3u,4u,"DifSpcBmp",33u,34u,35u,"45b985a46381199775b0089e9ac6ad5ea14a97b215421bfe29c29bd7ad97535a",false},
    {"mr.full24.mtd.ps_body_dsb_alp","Ps_Body[DSB]_Alp.mtd",3u,1.000000f,0u,7u,"DifSpcBmp",33u,34u,35u,"024f15bad3c22cd7476c82229b429eb81c3ea01626c18f8887e98f802f390aab",false},
    {"mr.full24.mtd.c_2320_metal_dsb","C_2320_Metal[DSB].mtd",4u,2.500000f,3u,4u,"DifSpcBmp",33u,34u,35u,"03d46faee57fd70c3af82939802bb1c2115aa62f80003346512276d1b9bd5e0a",false},
    {"mr.full24.mtd.p_metal_dsb_edge","P_Metal[DSB]_Edge.mtd",5u,2.500000f,3u,4u,"DifSpcBmp",33u,34u,35u,"8b730ce8655401c5cae694ba5535efb088d9d67db2a157d8f142cb5629293485",true},
    {"mr.full24.mtd.s_metal_dsb_edge","S_Metal[DSB]_Edge.mtd",5u,2.500000f,3u,4u,"DifSpcBmp",33u,34u,35u,"8b730ce8655401c5cae694ba5535efb088d9d67db2a157d8f142cb5629293485",true},
    {"mr.full24.mtd.p_roughcloth_dsb","P_RoughCloth[DSB].mtd",6u,1.000000f,0u,7u,"DifSpcBmp",33u,34u,35u,"db197a28abd96f55ce54da65dfea74e8788464dd6ae13ca7ceee038c56d281b1",false},
    {"mr.full24.mtd.p_roughcloth_dsb_edge","P_RoughCloth[DSB]_Edge.mtd",7u,1.000000f,0u,7u,"DifSpcBmp",33u,34u,35u,"4b3451c1d83d31f5ac1aad11bb5a16985ce49928bca3fcc10005032c784e45b8",false},
    {"mr.full24.mtd.p_wet_dsb","P_Wet[DSB].mtd",8u,2.000000f,0u,7u,"DifSpcBmp",33u,34u,35u,"7af76c9ed5adbb22a574b2afa382a10dfec37dc17470a3d9ea47be3bfb1ea97b",false},
    {"mr.full24.mtd.p_wet_dsb_edge","P_Wet[DSB]_Edge.mtd",9u,2.000000f,0u,7u,"DifSpcBmp",33u,34u,35u,"a7f73fd656797fea6eeb59131a5bd5c25198ad5c654524dadf76798c57aebb86",false},
    {"mr.full24.mtd.c_dullleather_dsb_l","C_DullLeather[DSB][L].mtd",10u,1.000000f,0u,7u,"DifSpcBmp Lit",30u,31u,32u,"6352d64c658a78c3c5c3a0bf20ca0da0726ccab88573a2c0d3b70fba6573fd5c",false},
    {"mr.full24.mtd.c_dullleather_dsb_l_alp","C_DullLeather[DSB][L]_Alp.mtd",11u,1.000000f,0u,7u,"DifSpcBmp Lit",30u,31u,32u,"af64a2f6fa374e2d8fce6b701827c31df30227d9bd076e07180b829a7bc487e3",false},
    {"mr.full24.mtd.c_leather_dsb_alp","C_Leather[DSB]_Alp.mtd",12u,1.500000f,0u,7u,"DifSpcBmp",33u,34u,35u,"9ef69c874a7ceff3797a3513026677d131f622f3f8476015a5ce4b70fcd6e190",false},
    {"mr.full24.mtd.c_metal_dsb_alp","C_Metal[DSB]_Alp.mtd",13u,2.500000f,3u,4u,"DifSpcBmp",33u,34u,35u,"45eb950965998f67d2d2b62fbcfc841f07e71e24097e1c6ceb97758752a651f0",false},
    {"mr.full24.mtd.c_roughcloth_dsb","C_RoughCloth[DSB].mtd",14u,1.000000f,0u,7u,"DifSpcBmp",33u,34u,35u,"9885c34b18f247b9f4446dbaeddef427e2058597d58e6297a1b9e6e8e21b8e51",false},
    {"mr.full24.mtd.c_roughcloth_dsb_edge","C_RoughCloth[DSB]_Edge.mtd",15u,1.000000f,0u,7u,"DifSpcBmp",33u,34u,35u,"0a29732f36dc7ba939bd0114c02af09e5b72a11745c76f90e9ed4991752994bd",false},
    {"mr.full24.mtd.c_wet_dsb_m","C_Wet[DSB][M].mtd",16u,2.000000f,0u,7u,"DifSpcBmp Mul",27u,28u,29u,"5910889ba3c575a33593b4035a3b50351c95ba7e687039424fa9d317b0ea788b",false},
    {"mr.full24.mtd.c_wet_dsb_alp","C_Wet[DSB]_Alp.mtd",17u,2.000000f,0u,7u,"DifSpcBmp",33u,34u,35u,"9c46069d7ee8c7e964704d5e805594d1cdf0263123e0f68c65ff38ef4420e7d9",false},
    {"mr.full24.mtd.c_leather_dsb","C_Leather[DSB].mtd",41u,1.500000f,0u,7u,"DifSpcBmp",33u,34u,35u,"236bb0a0614e4bf3326d610915f8b22c3cacb09ee1c0523a51fa08200dfbe524",false},
    {"mr.full24.mtd.c_leather_dsb_edge","C_Leather[DSB]_Edge.mtd",44u,1.500000f,0u,7u,"DifSpcBmp",33u,34u,35u,"25ffd8635203147f291c8f8af945dbad1898a1b5dfdcf31bb4136083fbda4d39",false},
    {"mr.full24.mtd.c_5350_body_dsb","C_5350_body[DSB].mtd",60u,1.000000f,0u,7u,"DifSpcBmp",33u,34u,35u,"2e14e4f8b7db61eb15a8682c577ae74842dfe680cc677063e8b31ca385b0902f",false},
    {"mr.full24.mtd.p_dullleather_dsb_edge","P_DullLeather[DSB]_Edge.mtd",88u,1.000000f,0u,7u,"DifSpcBmp",33u,34u,35u,"44e1188bafa99d49349883dcc2052c13365f196e341ca5e7c788cbad0a4e058d",false},
    {"mr.full24.mtd.p_dullleather_dsb","P_DullLeather[DSB].mtd",160u,1.000000f,0u,7u,"DifSpcBmp",33u,34u,35u,"7ea7d6ce9bb7148db62e8c27ee45edefb9547018d0b723dd4753d4c37e29f293",false},
    {"mr.full24.mtd.c_3530_unique_wet_dsb_edge","C_3530_Unique_Wet[DSB]_Edge.mtd",167u,2.000000f,0u,7u,"DifSpcBmp",33u,34u,35u,"833bb7b09f0dd02a61c75e9edbb76ad5ec14bdab44d663c1c652f53963eee963",false},
    {"mr.full24.mtd.c_5290_body_dsb","C_5290_Body[DSB].mtd",190u,2.000000f,0u,7u,"DifSpcBmp",33u,34u,35u,"915560f2453b1a385abf51b5d0675a61d1b31b52d16714484b57830c8e6e4253",false},
    {"mr.full24.mtd.c_wet_dsb_edge","C_Wet[DSB]_Edge.mtd",197u,2.000000f,0u,7u,"DifSpcBmp",33u,34u,35u,"94fb44042dee45d9b022b51ee5a030382ad7d4b7b10d7ec44e213f06fa1f377a",false},
    {"mr.full24.mtd.c_metal_dsb","C_Metal[DSB].mtd",229u,2.500000f,3u,4u,"DifSpcBmp",33u,34u,35u,"ae2e8df867fe2859eef37104c13c939e7e7fe4f703b7fe49c408d0230fc71d85",false},
    {"mr.full24.mtd.c_3530_unique_wet_dsb","C_3530_Unique_Wet[DSB].mtd",231u,2.000000f,0u,7u,"DifSpcBmp",33u,34u,35u,"ae7f795b03dcb2c204948e9a8e3b2798abe1c9b8912812e3817c095b07e5cd8f",false},
    {"mr.full24.mtd.ps_body_dsb","Ps_Body[DSB].mtd",232u,1.000000f,0u,7u,"DifSpcBmp",33u,34u,35u,"af2f108831b783a43b0e02f047919719d14f38e68d6c5a97b80678d593ba1c95",false},
    {"mr.full24.mtd.c_dullleather_dsb_alp","C_DullLeather[DSB]_Alp.mtd",276u,1.000000f,0u,7u,"DifSpcBmp",33u,34u,35u,"c1b1baca69cc33b449f7ff06bb08a959a1b74c90862d67acd003194062306dff",false},
    {"mr.full24.mtd.ps_facegen_ds_m","Ps_FaceGen[DS][M].mtd",312u,1.000000f,0u,7u,"DifSpcBmp Mul",27u,28u,29u,"d3854175a7b2969366ae346d686c0ea36f712a5097a5994b1a604cc64db82607",false},
    {"mr.full24.mtd.c_dullleather_dsb_edge","C_DullLeather[DSB]_Edge.mtd",331u,1.000000f,0u,7u,"DifSpcBmp",33u,34u,35u,"e2d819b90e8201a93e67e84e1179e9c3bf3f57988d5f86b44c5e024d05dee1a1",false},
    {"mr.full24.mtd.p_metal_dsb","P_Metal[DSB].mtd",345u,2.500000f,3u,4u,"DifSpcBmp",33u,34u,35u,"ece70f36bd2517d28c8495e276cea537f8b519d6bed981788e79a409ffbf763b",false},
    {"mr.full24.mtd.c_metal_dsb_edge","C_Metal[DSB]_Edge.mtd",359u,2.500000f,3u,4u,"DifSpcBmp",33u,34u,35u,"f797c63ac4f476ab7ebb7bc150ecc2a9c61851999f5051ceb864b62c95d924d9",false},
};

inline constexpr std::size_t k_material_route_count_v1 =
    sizeof(k_material_routes_v1) / sizeof(k_material_routes_v1[0]);

} // namespace dsrrl::operators::material_response::generated
