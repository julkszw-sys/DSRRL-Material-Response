#pragma once
// PTDE Classic/legacy EnvDiffuse DSRLEDF1 immutable corpus validator.
// Do not infer cross-version homology or alias the 342 GI_EnvDif probes.
#include "dsrrl/operators/env_spec/ptde_classic_envdiffuse_dds_sidecar_v1.hpp"
#include "dsrrl/operators/legacy_plan/sha256_bytes.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cstdio>

namespace dsrrl::operators::env_spec::classic_envdiffuse_sidecar_v1 {
using source_record=source_identity;
using source_index=std::array<source_record,k_records>;

inline std::uint32_t u32(const std::uint8_t *p) noexcept {
    return static_cast<std::uint32_t>(p[0]) |
       (static_cast<std::uint32_t>(p[1])<<8u) |
       (static_cast<std::uint32_t>(p[2])<<16u) |
       (static_cast<std::uint32_t>(p[3])<<24u);
}
inline std::uint64_t u64(const std::uint8_t *p) noexcept {
    return static_cast<std::uint64_t>(u32(p)) |
        (static_cast<std::uint64_t>(u32(p+4u))<<32u);
}
inline bool name_valid(const source_record &r) noexcept {
    bool terminated=false;
    for(std::size_t i=0u;i<sizeof(r.name);++i) {
        if(r.name[i]=='\0') {terminated=true;break;}
        if(r.name[i]<33 || r.name[i]>126) return false;
    }
    if(!terminated || std::memcmp(r.name,"EnvDif_",7u)!=0) return false;
    if(r.map_id==0xffffu)
        return std::strcmp(r.name,"EnvDif_def_000")==0 && r.legacy_row==0u;
    if(r.map_id<10u || r.map_id>18u || r.legacy_row>999u) return false;
    char expected[40]{};
    // No aliases, GI_ prefix, or noncanonical spelling.
    std::snprintf(expected,sizeof(expected),"EnvDif_m%u_%03u",
                  static_cast<unsigned>(r.map_id),
                  static_cast<unsigned>(r.legacy_row));
    return std::strcmp(r.name,expected)==0;
}
inline bool validate(const std::uint8_t *p,std::size_t size,
                     source_index &out) noexcept {
    out={};
    if(p==nullptr || size!=k_file_bytes ||
       std::memcmp(p,k_magic,8u)!=0 ||
       u32(p+8u)!=1u || u32(p+12u)!=k_records ||
       u64(p+16u)!=k_header_bytes || u64(p+24u)!=k_data_offset)
        return false;
    if(legacy_plan::hashing::sha256(p,size)!=k_exact_sha256)
        return false;
    std::size_t next=k_data_offset;
    for(std::size_t i=0u;i<k_records;++i) {
        source_record r{};
        std::memcpy(&r,p+k_header_bytes+i*k_record_bytes,sizeof(r));
        if(!name_valid(r) || r.cube_flag!=1u ||
           (r.format!=1u && r.format!=2u) ||
           r.width!=r.height || (r.width!=8u && r.width!=32u && r.width!=64u) ||
           r.mip_levels==0u || r.mip_levels>7u ||
           (r.format==1u && r.mip_levels!=1u) ||
           (r.format==2u && (r.width!=64u || r.mip_levels!=7u)) ||
           r.dds_offset!=next || r.dds_bytes<128u ||
           r.dds_bytes>size-next ||
           std::memcmp(p+next,"DDS ",4u)!=0 ||
           u32(p+next+4u)!=124u ||
           u32(p+next+12u)!=r.height ||
           u32(p+next+16u)!=r.width ||
           u32(p+next+28u)!=r.mip_levels ||
           (u32(p+next+112u)&0xfe00u)!=0xfe00u)
           return false;
        for(const auto b:r.reserved) if(b!=0u) return false;
        const auto *dds=p+next;
        std::size_t payload=0u;
        if(r.format==1u) {
            if(u32(dds+88u)!=24u || u32(dds+92u)!=0xff0000u ||
               u32(dds+96u)!=0xff00u || u32(dds+100u)!=0xffu ||
               u32(dds+104u)!=0u) return false;
            payload=static_cast<std::size_t>(r.width)*r.height*3u*6u;
        } else {
            if(std::memcmp(dds+84u,"DXT1",4u)!=0) return false;
            for(std::uint32_t mip=0u;mip<r.mip_levels;++mip) {
                const auto side=static_cast<std::size_t>(r.width)>>mip;
                const auto blocks=((side+3u)/4u);
                payload+=blocks*blocks*8u*6u;
            }
        }
        if(r.dds_bytes!=128u+payload ||
           legacy_plan::hashing::sha256(dds,static_cast<std::size_t>(r.dds_bytes))
              !=r.dds_sha256) return false;
        for(std::size_t j=0u;j<i;++j)
            if(std::strcmp(r.name,out[j].name)==0) return false;
        out[i]=r;
        next+=static_cast<std::size_t>(r.dds_bytes);
    }
    return next==size;
}
inline const source_record *find(const source_index &rows,
                                 const char *exact_name) noexcept {
    if(exact_name==nullptr) return nullptr;
    const source_record *result=nullptr;
    for(const auto &r:rows) {
        if(std::strcmp(r.name,exact_name)==0) {
            if(result!=nullptr) return nullptr;
            result=&r;
        }
    }
    return result;
}
} // namespace dsrrl::operators::env_spec::classic_envdiffuse_sidecar_v1
