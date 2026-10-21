#pragma once
// V363 independent, diagnostic-only original retail legacy EnvSpec name.
// Captures solely the synchronous texture loader TLS signal. This is NEVER
// a probe authorization, light/material modification, or t11/t12 alias.
#include <array>
#include <cstddef>
#include <cstdint>
#include "dsrrl/runtime/texture_identity_transport.hpp"
namespace dsrrl::runtime::gi_envspec_name_witness {
struct identity {
    std::array<char,97> name{};
    unsigned stage = 0u;
    bool conflicting = false;
};
inline bool capture(identity &out) noexcept {
    out={};
    const wchar_t *p=nullptr;
    std::size_t len=0u;
    if (!texture_identity_transport::snapshot_raw(p,len) ||
        p==nullptr || len<10u || len>96u) return false;
    constexpr char legacy[]="EnvSpc_";
    for(std::size_t i=0u;i<sizeof(legacy)-1u;++i)
        if(p[i]!=static_cast<wchar_t>(legacy[i]))return false;
    for(std::size_t i=0u;i<len;++i) {
        const wchar_t c=p[i];
        if(!((c>=L'A'&&c<=L'Z') ||
             (c>=L'a'&&c<=L'z') ||
             (c>=L'0'&&c<=L'9') || c==L'_'))
            return false;
        out.name[i]=static_cast<char>(c);
    }
    out.name[len]=0;
    return true;
}
} // namespace dsrrl::runtime::gi_envspec_name_witness
