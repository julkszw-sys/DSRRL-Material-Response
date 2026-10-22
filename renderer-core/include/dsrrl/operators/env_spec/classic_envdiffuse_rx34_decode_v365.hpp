#pragma once
// V365: exact source-identity-scoped independent RX34 Classic EnvDiffuse
// post-filter RGB/A shader island. STAGING ONLY, not runtime activated.
// PTDE Classic RGB24 alpha=1; this materializer does not solve LightBank
// endpoint homology, native material ownership, or PTDE pixels.
// This module touches solely the t11/s11 sample/decode; retains stock t12/t14,
// all EnvSpec/PBL/specular/material/PointLight/terminal instructions.
#include "dsrrl/operators/legacy_plan/dxbc_checksum.hpp"
#include "dsrrl/operators/legacy_plan/sha256_bytes.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <vector>

namespace dsrrl::operators::env_spec::classic_envdiffuse_rx34_v365 {
enum class result : std::uint8_t {
    candidate_ready,
    not_exact_rx34,
    invalid_dxbc,
    nonunique_source_cut,
    declared_temp_mismatch,
    rebuild_failed
};

inline constexpr std::size_t k_stock_bytes=19556u;
inline constexpr std::size_t k_candidate_bytes=k_stock_bytes+48u;
inline constexpr std::array<std::uint8_t,32> k_stock_sha256{{
  0xd6,0x03,0x8d,0xe4,0x94,0x50,0x9e,0x7c,
  0xbc,0xbf,0xb9,0x04,0xc4,0x04,0x6e,0x94,
  0x27,0xf3,0xb9,0x21,0xf6,0xa3,0x57,0x35,
  0xa0,0xb0,0xd3,0x16,0xf9,0x97,0x68,0x37
}};

// Exact original unmodified DSR RX34 t11 window (source @ 1596,
// sample_l @ 1605, merge @ 1618, SHEX 32-bit word offsets).
inline constexpr std::array<std::uint32_t,9> k_dsr_source{{
    0x09000038u,0x00100072u,2u,
    0x00208246u,0u,3u,0x00208006u,0u,79u
}};
inline constexpr std::array<std::uint32_t,13> k_dsr_t11_sample{{
    0x8d000048u,0x80000182u,0x00155543u,
    0x00100072u,3u,0x00100246u,4u,
    0x00107e46u,11u,0x00106000u,11u,
    0x00004001u,0u
}};
inline constexpr std::array<std::uint32_t,7> k_dsr_merge{{
    0x07000038u,0x00100072u,2u,
    0x00100246u,2u,0x00100246u,3u
}};

// Dedicated fresh r13 is added by increasing exact dcl_temps 13 -> 14.
// The source shader never references r13: it declares exactly r0..r12.
// A sample wxyz swizzle places filtered alpha into r13.x and RGB in yzw.
// DIV computes r13.xyz = r13.yzw / r13.x after filtering.
// MOV writes original sample destination r3.xyz, preserves original r3.w.
// Stock merge r2.xyz *= r3.xyz follows, preserving DSR LightBank/source
// and ALL independent stock DSR operators until separately authorized.
inline constexpr std::array<std::uint32_t,12> k_ptde_rgb_div_and_mov{{
    0x0700000eu,0x00100072u,13u,
    0x00100e56u,13u,0x00100006u,13u,
    0x05000036u,0x00100072u,3u,0x00100246u,13u
}};

inline std::uint32_t load32(const std::uint8_t *p) noexcept {
    return legacy_plan::dxbc::read_u32(p);
}
inline void store32(std::uint8_t *p,std::uint32_t v) noexcept {
    legacy_plan::dxbc::write_u32(p,v);
}
template<std::size_t N>
inline bool equals(const std::vector<std::uint32_t> &v,
                   std::size_t at,const std::array<std::uint32_t,N> &a) noexcept {
    if(at>v.size() || N>v.size()-at) return false;
    for(std::size_t i=0;i<N;++i) if(v[at+i]!=a[i]) return false;
    return true;
}
struct chunk {
    std::array<std::uint8_t,4> tag{};
    std::vector<std::uint8_t> payload;
};
inline result materialize(const std::uint8_t *stock,
                         std::size_t n,std::vector<std::uint8_t> &out) noexcept {
    out.clear();
    if(stock==nullptr || n!=k_stock_bytes ||
       legacy_plan::hashing::sha256(stock,n)!=k_stock_sha256)
        return result::not_exact_rx34;
    if(!legacy_plan::dxbc::checksum_container_valid(stock,n))
        return result::invalid_dxbc;
    try {
        const auto count=load32(stock+28u);
        if(count!=5u || 32u+4u*count>n) return result::invalid_dxbc;
        std::vector<chunk> chunks;
        chunks.reserve(count);
        std::size_t shex_index=std::numeric_limits<std::size_t>::max();
        std::size_t sum_bytes=32u+4u*count;
        for(std::uint32_t i=0;i<count;++i) {
            const auto off=static_cast<std::size_t>(load32(stock+32u+4u*i));
            if(off>n || n-off<8u) return result::invalid_dxbc;
            const auto payload=static_cast<std::size_t>(load32(stock+off+4u));
            if(payload>n-off-8u || off!=sum_bytes)
                return result::invalid_dxbc;
            chunk c{};
            std::memcpy(c.tag.data(),stock+off,4u);
            c.payload.assign(stock+off+8u,stock+off+8u+payload);
            if(std::memcmp(c.tag.data(),"SHEX",4u)==0) {
                if(shex_index!=std::numeric_limits<std::size_t>::max())
                    return result::invalid_dxbc;
                shex_index=chunks.size();
            }
            sum_bytes=off+8u+payload;
            chunks.push_back(std::move(c));
        }
        if(sum_bytes!=n || shex_index==std::numeric_limits<std::size_t>::max())
            return result::invalid_dxbc;
        auto &code=chunks[shex_index].payload;
        if(code.size()%4u!=0u) return result::invalid_dxbc;
        std::vector<std::uint32_t> w(code.size()/4u);
        for(std::size_t i=0;i<w.size();++i) w[i]=load32(code.data()+i*4u);
        if(w.size()<1700u || w[1]!=w.size())
            return result::invalid_dxbc;

        // Decode ALL variable-length SHEX instructions, never find tokens in
        // random immediate operands. Exact source token window must be unique.
        std::size_t i=2u,site=0u,site_hits=0u,temps=0u,temp_hits=0u;
        while(i<w.size()) {
            const auto len=static_cast<std::size_t>((w[i]>>24u)&127u);
            const auto op=w[i]&0x7ffu;
            if(len==0u || len>w.size()-i)
                return result::invalid_dxbc;
            if(op==0x68u && len==2u) {
                ++temp_hits; temps=i;
                if(w[i]!=0x02000068u || w[i+1u]!=13u)
                    return result::declared_temp_mismatch;
            }
            if(equals(w,i,k_dsr_t11_sample)) {
                if(i<k_dsr_source.size() ||
                   !equals(w,i-k_dsr_source.size(),k_dsr_source) ||
                   !equals(w,i+k_dsr_t11_sample.size(),k_dsr_merge))
                    return result::nonunique_source_cut;
                ++site_hits;
                site=i;
            }
            i+=len;
        }
        if(i!=w.size() || site_hits!=1u)
            return result::nonunique_source_cut;
        if(temp_hits!=1u || temps>=site)
            return result::declared_temp_mismatch;

        // No PBL/EnvSpec/LightBank mutation. Source, sampling coords and
        // t11/s11 remain identical. New register is proven unused by dcl.
        w[temps+1u]=14u;
        w[site+3u]=0x001000f2u; // sample dest .xyzw
        w[site+4u]=13u;         // fresh temp r13
        w[site+7u]=0x00107936u; // sample.wxyz -> alpha,RGB
        w.insert(w.begin()+static_cast<std::ptrdiff_t>(
                     site+k_dsr_t11_sample.size()),
                 k_ptde_rgb_div_and_mov.begin(),
                 k_ptde_rgb_div_and_mov.end());
        w[1]=static_cast<std::uint32_t>(w.size());
        code.resize(w.size()*4u);
        for(std::size_t j=0;j<w.size();++j)store32(code.data()+4u*j,w[j]);

        // Rebuild 5 original chunks with identical RDEF/ISGN/OSGN/STAT.
        // SHEX alone grows 48 bytes; update offsets, size, DXBC checksum.
        const auto header_bytes=32u+4u*count;
        out.assign(stock,stock+header_bytes);
        for(std::size_t j=0;j<chunks.size();++j) {
            if(out.size()>std::numeric_limits<std::uint32_t>::max())
                return result::rebuild_failed;
            store32(out.data()+32u+4u*j,
                    static_cast<std::uint32_t>(out.size()));
            const auto &c=chunks[j];
            out.insert(out.end(),c.tag.begin(),c.tag.end());
            const auto at=out.size();
            out.resize(at+4u);
            store32(out.data()+at,static_cast<std::uint32_t>(c.payload.size()));
            out.insert(out.end(),c.payload.begin(),c.payload.end());
        }
        if(out.size()!=k_candidate_bytes)
            return result::rebuild_failed;
        store32(out.data()+24u,static_cast<std::uint32_t>(out.size()));
        std::memset(out.data()+4u,0,16u);
        if(!legacy_plan::dxbc::fix_checksum(out.data(),out.size()) ||
           !legacy_plan::dxbc::checksum_container_valid(out.data(),out.size())) {
            out.clear();return result::rebuild_failed;
        }
        return result::candidate_ready;
    } catch (...) {
        out.clear();
        return result::rebuild_failed;
    }
}
} // namespace dsrrl::operators::env_spec::classic_envdiffuse_rx34_v365
