#pragma once
// V373: SOURCE CENSUS ONLY. These 17 exact DSR name+creation-SHA pairs
// were observed in owner V364 runtime log SHA256:
// 56a6702b61815120e8e1d82ef3b6823f4691b16e77b24c33380767db695761d5
// All names occur in the owner-supplied original PTDE 39 DDS container.
// Original DDS asset identity DOES NOT authorize PTDE pixel matching.
// ONLY m18_500 and m18_510 remain bridge-authorized by V364.
// No shader, resources or material gate is widened by this manifest.
#include <array>
#include <cstdint>
#include <cstring>
#include "dsrrl/operators/legacy_plan/sha256_bytes.hpp"

namespace dsrrl::runtime::v373_legacygi {
struct candidate {
    const char *name;
    const char *native_creation_sha256;
    bool previously_authorized;
};
inline constexpr std::array<candidate,17u> k_candidates{{
    {"EnvDif_m10_000", "03c29797d86471fab4c79f2ff8f3c1ce50e6375cd26d86a9d58d9c3fd62fc55d", false},
    {"EnvDif_m10_010", "debd7798f2e5c36c854757755670cb4a3b9e3555b92ccd8ab28ca6e0fed3e27f", false},
    {"EnvDif_m10_020", "4f7c97714af6848a254d937dc8a6d42e683cbb504239cd81da0e0dc22b3481ed", false},
    {"EnvDif_m10_030", "12da8820e37c6083f6de046e420b1c342ffe82783ee32fb236e5f95c766352ad", false},
    {"EnvDif_m10_040", "cb4379c7432e9c6d0c1ef898fed21ecaa7dbf7762fb5dae0e91cc889aac94f02", false},
    {"EnvDif_m10_090", "222c67535e1de56077260ff0c180987c8df017df8df1288d27dd94a4ed28f8dd", false},
    {"EnvDif_m10_100", "95132b502e0afcefcd4aa0fe7b6849a7a8c90d29c8dc677e4e7e5b056fa8b200", false},
    {"EnvDif_m10_200", "e606793697a00e43df5d831095cd3936d81e3995dcbbe95a238fd3d838b7556b", false},
    {"EnvDif_m10_500", "e1a51e6c3f0163ccae1cc4dba4a094dc166fbf375ec1b1744f3b00e4c27bd418", false},
    {"EnvDif_m18_000", "33e2202b14b5161d151722305ac9d9bb2aa76ac62209e15aee290abfd2a34821", false},
    {"EnvDif_m18_100", "5f104202a2fc7582b8edf2ea08c485147d8394a58b23940b0536d82bcdf75399", false},
    {"EnvDif_m18_160", "600a0aa98d0c7a0367546942eaa99496650518c29a751e4558e84ecb93627daf", false},
    {"EnvDif_m18_170", "903c78478378234c0d72fe6e6671ff01ddb49a220a9b1d9b963b0707406c1785", false},
    {"EnvDif_m18_180", "de956be56e702519434b4a5cec0ddd52c143cf95c91b66467e0d1c36c9fd1c66", false},
    {"EnvDif_m18_190", "648ad5c11f82220a48ca228ef2d064119e9615af755e9e83ace99659498f2cf3", false},
    {"EnvDif_m18_500", "19241a18f938ea8fa00dcaede57e9116c60cf818d5c8faaa1cc93e7eb98ce81b", true},
    {"EnvDif_m18_510", "cace4d9bdbe2bd54bb9b4e7c91002b88c6a68c367638febde9c76144ee5a7eb9", true},
}};
inline int find_exact(const char *name,
    const std::array<std::uint8_t,32> &sha) noexcept {
    if(!name || !name[0]) return -1;
    for(unsigned i=0;i<k_candidates.size();++i) {
        const auto &c=k_candidates[i];
        if(std::strcmp(name,c.name)==0 &&
          dsrrl::operators::legacy_plan::hashing::matches_hex(
              sha,c.native_creation_sha256))return static_cast<int>(i);
    }
    return -1;
}
} // namespace dsrrl::runtime::v373_legacygi
