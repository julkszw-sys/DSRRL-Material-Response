#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace dsrrl::runtime::mr::pmetal_envspec_rgba_authority {

// Source-complete retarget of the exact Build131 P_Metal RGBA operator island.
// Historical generator commit:
//   780a89f79dbd32c09c7f3738de879ce507e83d5b
// Historical basis/output:
//   e15747f4920bd5f40db9019e4e45110ad4655b4defab1ad3027b039fc022d98b
//   db2e6547b5fb5516d4ad6559173e66421315d1635ca0c08e8141b0de2f57f966
//
// Build131 changed only dedicated DXBC72/73/74 and proved:
//   raw RGBA t12/t14 SAMPLE_L LOD0 -> RGB/sample.alpha decode ->
//   PTDE endpoint source factors + beta; downstream fresh t10*c101*COLOR0
//   remains outside this 100-word window.
//
// Current runtime does not import the old PE or binary shader sidecars.  The
// recovered window is applied only to the exact current V2.11 P_Metal bodies
// already certified by v13_authority. Structural pre/postconditions in the
// transformer fail open if that semantic island no longer matches.
struct entry {
    std::string_view input_v211_sha256;
    std::size_t t12_word;
    std::size_t merge_word;
    std::uint32_t reflection_coord_register;
    std::string_view historical_pre_sha256;
    std::string_view historical_post_sha256;
};

inline constexpr std::array<entry,3> k_entries = {{
    {
        "70b85d49cea116ff1f72a3fd5bb7726ee72aee5a0655cc81718a83a659ee03f5",
        1605u,1720u,7u,
        "bf5da767361e5d3d4354de2c30b67e68029ac865fe4823c61ae6d25d273da49f",
        "159e9bbcb36c110e0e6e223f740986844cc8f0882d472afaf5b65abee4301e98"
    },
    {
        "ec7f133592462d58c2715c84a398f70f60cecaec8eaaa436ace62b5a262a3ae3",
        1514u,1629u,6u,
        "4c79a8eb7b8aa744f756a04b1af968a1da105726914ed420feb7424334c31566",
        "726461e5308788f75e7dba4e0e7c85e9fcc801faec38c060635dff40fb9ae877"
    },
    {
        "ae69bfe343cb913c4eac4e1d07d1280e4c4dcf02080d1cd55305841e21f76757",
        1174u,1289u,5u,
        "3c0660946fa93a6c1a4b744a7fe31348ae0b62b19b1e7da3372782fd337530e9",
        "11a405536a0600037b11817579a29cf5663bc62e8b17955af255d279e03417c7"
    }
}};

constexpr const entry *find(std::string_view sha) noexcept
{
    for(const auto &e:k_entries)
        if(e.input_v211_sha256==sha)
            return &e;
    return nullptr;
}

} // namespace dsrrl::runtime::mr::pmetal_envspec_rgba_authority
