#pragma once
#include <array>
#include <cstddef>
#include <string_view>

namespace dsrrl::runtime::mr::v13_authority {

struct entry {
    std::string_view input_v211_sha256;
    std::string_view output_v13_sha256;
    std::size_t t12_word;
    std::size_t merge_word;
};

inline constexpr std::array<entry,3> k_pmetal = {{
    {"70b85d49cea116ff1f72a3fd5bb7726ee72aee5a0655cc81718a83a659ee03f5",
     "f6340b44edb4364a23690d13f6d0539c4c2daf99ce98c2ae5c901121e0c8f926",
     1605u,1720u},
    {"ec7f133592462d58c2715c84a398f70f60cecaec8eaaa436ace62b5a262a3ae3",
     "411900aadafb9ccd3bb777aff6382c7e3c938807274526aa4adaa5bb6cb2fc20",
     1514u,1629u},
    {"ae69bfe343cb913c4eac4e1d07d1280e4c4dcf02080d1cd55305841e21f76757",
     "098f5d364413bb9ae5519d58e5ee92f90dd5d63d4c7f8179759aeccbb635b00a",
     1174u,1289u}
}};

constexpr const entry *find(std::string_view sha) noexcept
{
    for(const auto &e:k_pmetal)
        if(e.input_v211_sha256==sha)
            return &e;
    return nullptr;
}

} // namespace dsrrl::runtime::mr::v13_authority
