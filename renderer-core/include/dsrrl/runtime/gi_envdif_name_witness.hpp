#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include "dsrrl/runtime/texture_identity_transport.hpp"

namespace dsrrl::runtime::gi_envdif_name_witness {
struct identity {
    std::array<char,97> name{};
    unsigned stage=0u;
    bool conflicting=false;
};
inline bool capture(identity &out) noexcept {
    out={};
    const wchar_t *p=nullptr;
    std::size_t len=0u;
    if (!texture_identity_transport::snapshot_raw(p,len) ||
        p==nullptr || len>96u || len<8u) return false;
    const char *packed="GI_EnvDif_";
    const char *legacy="EnvDif_";
    const auto prefix=[&](const char *candidate) noexcept {
        std::size_t i=0u;
        while(candidate[i]!=0) {
            if(i>=len || p[i]!=candidate[i])return false;
            ++i;
        }
        return i<len;
    };
    if (!prefix(packed) && !prefix(legacy))return false;
    for(std::size_t i=0u;i<len;++i) {
        const wchar_t c=p[i];
        if (!((c>=L'A'&&c<=L'Z')||(c>=L'a'&&c<=L'z')||
              (c>=L'0'&&c<=L'9')||c==L'_'))return false;
        out.name[i]=static_cast<char>(c);
    }
    out.name[len]=0;
    return true;
}
} // namespace dsrrl::runtime::gi_envdif_name_witness
