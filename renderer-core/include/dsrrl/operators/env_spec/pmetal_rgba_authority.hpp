#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace dsrrl::operators::env_spec::pmetal_rgba_authority {

struct entry {
    std::uint32_t receiver_id = 0;
    std::string_view stock_sha256{};
    std::size_t t12_word = 0;
    std::size_t merge_word = 0;
    std::uint32_t reflection_coord_register = 0;
    std::size_t spec_log_word = 0;
    std::size_t spec_gamma_mul_word = 0;
    std::size_t spec_exp_word = 0;
    std::uint32_t color0_register = 0;
    std::size_t ul_u_slot_word = 0;
    std::size_t ul_d_slot_word_0 = 0;
    std::size_t ul_d_slot_word_1 = 0;
    bool remove_visibility_exponent = false;
    std::string_view historical_pre_sha256{};
    std::string_view historical_post_sha256{};
};
inline constexpr std::array<entry,3> k_entries = {{
    {33u,
     "35880c0b2f2330208dfc21af6dd3d944218fcc4540cd8e59404a0aefc13c0b24",
     1605u,1720u,7u,1320u,1325u,1335u,6u,1745u,1749u,1759u,true,
     "bf5da767361e5d3d4354de2c30b67e68029ac865fe4823c61ae6d25d273da49f",
     "159e9bbcb36c110e0e6e223f740986844cc8f0882d472afaf5b65abee4301e98"},
    {34u,
     "d6038de494509e7cbcbfb904c4046e9427f3b921f6a35735a0b0d316f9976837",
     1514u,1629u,6u,1229u,1234u,1244u,7u,1654u,1658u,1668u,true,
     "4c79a8eb7b8aa744f756a04b1af968a1da105726914ed420feb7424334c31566",
     "726461e5308788f75e7dba4e0e7c85e9fcc801faec38c060635dff40fb9ae877"},
    {35u,
     "7d03c75b69f5730eb741a4d327189d0bbed8a8450fb0ac04e1505f7b91763701",
     1174u,1289u,5u,889u,894u,904u,6u,1307u,1311u,1321u,false,
     "3c0660946fa93a6c1a4b744a7fe31348ae0b62b19b1e7da3372782fd337530e9",
     "11a405536a0600037b11817579a29cf5663bc62e8b17955af255d279e03417c7"}
}};

constexpr const entry *find(std::string_view sha) noexcept
{
    for (const auto &e : k_entries)
        if (e.stock_sha256 == sha)
            return &e;
    return nullptr;
}

} // namespace dsrrl::operators::env_spec::pmetal_rgba_authority
