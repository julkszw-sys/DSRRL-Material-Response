#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace dsrrl::runtime::generated {

struct spec_material_route { std::string_view sha256; std::uint16_t route_index; std::array<std::uint32_t,3> receivers; };

inline constexpr std::array<spec_material_route, 34> k_spec_material_routes = {{
    {"024f15bad3c22cd7476c82229b429eb81c3ea01626c18f8887e98f802f390aab",3u,{{33u,34u,35u}}},
    {"03d46faee57fd70c3af82939802bb1c2115aa62f80003346512276d1b9bd5e0a",4u,{{33u,34u,35u}}},
    {"0a29732f36dc7ba939bd0114c02af09e5b72a11745c76f90e9ed4991752994bd",15u,{{33u,34u,35u}}},
    {"236bb0a0614e4bf3326d610915f8b22c3cacb09ee1c0523a51fa08200dfbe524",41u,{{33u,34u,35u}}},
    {"25ffd8635203147f291c8f8af945dbad1898a1b5dfdcf31bb4136083fbda4d39",44u,{{33u,34u,35u}}},
    {"2e14e4f8b7db61eb15a8682c577ae74842dfe680cc677063e8b31ca385b0902f",60u,{{33u,34u,35u}}},
    {"44e1188bafa99d49349883dcc2052c13365f196e341ca5e7c788cbad0a4e058d",88u,{{33u,34u,35u}}},
    {"45b985a46381199775b0089e9ac6ad5ea14a97b215421bfe29c29bd7ad97535a",2u,{{33u,34u,35u}}},
    {"45eb950965998f67d2d2b62fbcfc841f07e71e24097e1c6ceb97758752a651f0",13u,{{33u,34u,35u}}},
    {"4b3451c1d83d31f5ac1aad11bb5a16985ce49928bca3fcc10005032c784e45b8",7u,{{33u,34u,35u}}},
    {"4c728a9b5957a75d0eb82b2b77b800829e1973632c7c0690a7e31a074c85e7fb",0u,{{33u,34u,35u}}},
    {"5910889ba3c575a33593b4035a3b50351c95ba7e687039424fa9d317b0ea788b",16u,{{27u,28u,29u}}},
    {"6352d64c658a78c3c5c3a0bf20ca0da0726ccab88573a2c0d3b70fba6573fd5c",10u,{{30u,31u,32u}}},
    {"7af76c9ed5adbb22a574b2afa382a10dfec37dc17470a3d9ea47be3bfb1ea97b",8u,{{33u,34u,35u}}},
    {"7ea7d6ce9bb7148db62e8c27ee45edefb9547018d0b723dd4753d4c37e29f293",160u,{{33u,34u,35u}}},
    {"833bb7b09f0dd02a61c75e9edbb76ad5ec14bdab44d663c1c652f53963eee963",167u,{{33u,34u,35u}}},
    {"8b730ce8655401c5cae694ba5535efb088d9d67db2a157d8f142cb5629293485",5u,{{33u,34u,35u}}},
    {"915560f2453b1a385abf51b5d0675a61d1b31b52d16714484b57830c8e6e4253",190u,{{33u,34u,35u}}},
    {"94fb44042dee45d9b022b51ee5a030382ad7d4b7b10d7ec44e213f06fa1f377a",197u,{{33u,34u,35u}}},
    {"9885c34b18f247b9f4446dbaeddef427e2058597d58e6297a1b9e6e8e21b8e51",14u,{{33u,34u,35u}}},
    {"9c46069d7ee8c7e964704d5e805594d1cdf0263123e0f68c65ff38ef4420e7d9",17u,{{33u,34u,35u}}},
    {"9ef69c874a7ceff3797a3513026677d131f622f3f8476015a5ce4b70fcd6e190",12u,{{33u,34u,35u}}},
    {"a7f73fd656797fea6eeb59131a5bd5c25198ad5c654524dadf76798c57aebb86",9u,{{33u,34u,35u}}},
    {"ae2e8df867fe2859eef37104c13c939e7e7fe4f703b7fe49c408d0230fc71d85",229u,{{33u,34u,35u}}},
    {"ae7f795b03dcb2c204948e9a8e3b2798abe1c9b8912812e3817c095b07e5cd8f",231u,{{33u,34u,35u}}},
    {"af2f108831b783a43b0e02f047919719d14f38e68d6c5a97b80678d593ba1c95",232u,{{33u,34u,35u}}},
    {"af64a2f6fa374e2d8fce6b701827c31df30227d9bd076e07180b829a7bc487e3",11u,{{30u,31u,32u}}},
    {"c1b1baca69cc33b449f7ff06bb08a959a1b74c90862d67acd003194062306dff",276u,{{33u,34u,35u}}},
    {"cf968a22722667777d10429a03a2e4a87f867412949a7fe5590cb8cefaa2bfa4",1u,{{33u,34u,35u}}},
    {"d3854175a7b2969366ae346d686c0ea36f712a5097a5994b1a604cc64db82607",312u,{{27u,28u,29u}}},
    {"db197a28abd96f55ce54da65dfea74e8788464dd6ae13ca7ceee038c56d281b1",6u,{{33u,34u,35u}}},
    {"e2d819b90e8201a93e67e84e1179e9c3bf3f57988d5f86b44c5e024d05dee1a1",331u,{{33u,34u,35u}}},
    {"ece70f36bd2517d28c8495e276cea537f8b519d6bed981788e79a409ffbf763b",345u,{{33u,34u,35u}}},
    {"f797c63ac4f476ab7ebb7bc150ecc2a9c61851999f5051ceb864b62c95d924d9",359u,{{33u,34u,35u}}},
}};
inline bool spec_material_route_allowed(std::string_view sha256,std::uint32_t receiver) noexcept
{
    for(const auto &r:k_spec_material_routes){
        if(r.sha256!=sha256) continue;
        return r.receivers[0]==receiver || r.receivers[1]==receiver || r.receivers[2]==receiver;
    }
    return false;
}
inline constexpr std::size_t k_spec_material_route_count=k_spec_material_routes.size();

} // namespace dsrrl::runtime::generated
