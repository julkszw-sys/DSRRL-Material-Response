#pragma once
// V314 read-only exact PS-SHA + original-MTD receiver census.
// This diagnostic NEVER grants shader/operator authority. Unknowns remain
// original stock DSR; no draw replay, SRV/CB/PS mutation or model alias.
// One log per observed shader/material pair; all allocations fail open.
#include "dsrrl/runtime/v314_original_lit_mul_mullit_sources.hpp"
#include "dsrrl/operators/material_response/mtd_semantic_census.hpp"
#include "dsrrl/operators/legacy_plan/sha256_bytes.hpp"
#include <reshade.hpp>
#include <array>
#include <atomic>
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <mutex>
#include <unordered_map>

namespace dsrrl::runtime::v314 {
namespace mr=dsrrl::operators::material_response;
struct shader_record {
    std::array<std::uint8_t,32> sha{};
    std::uint32_t byte_size=0u;
    bool conflict=false;
};
struct bound_record {
    std::uint64_t pipeline=0u;
    shader_record shader{};
};
inline std::mutex g_mutex;
inline std::unordered_map<std::uint64_t,shader_record> g_shaders;
inline std::unordered_map<const void*,bound_record> g_bound;
inline std::array<std::atomic<std::uint64_t>,8192> g_first{};
inline std::atomic<std::uint32_t> g_reported{0u};

inline void observe_pipeline(
    std::uint64_t pipeline,const void *bytecode,std::size_t bytes) noexcept {
    if(pipeline==0u||!bytecode||bytes<64u||bytes>65536u)return;
    // This is the actual DSR runtime pixel shader, not a guessed .SPX map.
    const auto sha=operators::legacy_plan::hashing::sha256(
        static_cast<const std::uint8_t*>(bytecode),bytes);
    try {
        std::lock_guard<std::mutex> lock(g_mutex);
        if(g_shaders.size()>8192u)return;
        auto [it,inserted]=g_shaders.emplace(pipeline,
           shader_record{sha,static_cast<std::uint32_t>(bytes),false});
        if(!inserted &&
           (it->second.sha!=sha||it->second.byte_size!=bytes))
           it->second.conflict=true; // handle reuse is NOT an identity
    }catch(...){}
}
inline void forget_pipeline(std::uint64_t pipeline) noexcept {
    if(!pipeline)return;
    try{
        std::lock_guard<std::mutex> lock(g_mutex);
        g_shaders.erase(pipeline);
        for(auto it=g_bound.begin();it!=g_bound.end();) {
            if(it->second.pipeline==pipeline)it=g_bound.erase(it);
            else ++it;
        }
    }catch(...){}
}
inline void observe_bind(const void*cmd,std::uint64_t pipeline) noexcept {
    if(!cmd)return;
    try {
        std::lock_guard<std::mutex> lock(g_mutex);
        auto it=g_shaders.find(pipeline);
        if(it==g_shaders.end()||it->second.conflict) {
            g_bound.erase(cmd);return;
        }
        if(g_bound.size()>=4096u && g_bound.find(cmd)==g_bound.end())
           return;
        g_bound[cmd]=bound_record{pipeline,it->second};
    }catch(...) {
        // Diagnostic state must never interfere with stock DSR.
    }
}
inline bool claim(std::uint64_t hash) noexcept {
    if(hash==0u)hash=1u;
    constexpr std::size_t mask=g_first.size()-1u;
    for(unsigned i=0;i<12u;++i){
        auto &c=g_first[(static_cast<std::size_t>(hash)+i*127u)&mask];
        auto v=c.load(std::memory_order_relaxed);
        if(v==hash)return false;
        if(v==0u && c.compare_exchange_strong(
             v,hash,std::memory_order_relaxed))return true;
    }
    return false;
}
inline bool find_original_source(
    const mr::material_identity &m,
    std::uint32_t &family,std::uint32_t &slot) noexcept {
    family=0u;slot=0u;
    if(!m.valid||!m.material_slot_valid||!m.owner_tuple_exact||
       !m.actual_material_exact||m.semantic_name_hash==0u)return false;
    unsigned matches=0u;
    for(const auto &r:k_sources){
        if(m.semantic_name_hash!=mr::mtd_semantic_hash(r.name)||
           !operators::legacy_plan::hashing::matches_hex(
               m.raw_mtd_sha256,r.sha))continue;
        matches++;
        family=r.family;slot=r.slot;
    }
    // A full raw SHA is necessary but not sufficient: exact original
    // basename comes independently from full FLVER SHA+material slot.
    return matches==1u;
}
inline void report_stock_unrouted(
    const void *cmd,
    const mr::material_identity &m,
    bool native_stock_t1) noexcept {
    if(!cmd||!native_stock_t1||
       g_reported.load(std::memory_order_relaxed)>=512u)return;
    bound_record observed{};
    try{
        std::lock_guard<std::mutex> lock(g_mutex);
        const auto it=g_bound.find(cmd);
        if(it==g_bound.end()||it->second.shader.conflict)return;
        observed=it->second;
    }catch(...){return;}
    std::uint32_t family=0u,slot=0u;
    const bool exact=find_original_source(m,family,slot);
    // The PS digest is a real DSR pipeline witness. The original SPX
    // family is known ONLY when full FLVER owner+current MTD parser match.
    std::uint64_t key=14695981039346656037ull;
    const auto mix=[&](std::uint8_t x) noexcept {
        key=(key^static_cast<std::uint64_t>(x))*1099511628211ull;
    };
    for(auto b:observed.shader.sha)mix(b);
    for(auto b:m.raw_mtd_sha256)mix(b);
    for(auto b:m.flver_sha256)mix(b);
    for(unsigned n:{family,slot,m.material_slot})
        for(unsigned j=0;j<4u;++j)mix(static_cast<std::uint8_t>(n>>(8u*j)));
    if(!claim(key))return;
    if(g_reported.fetch_add(1u)>=512u)return;
    static constexpr char hex[]="0123456789abcdef";
    char ps[65]{};
    for(unsigned i=0;i<32u;++i){
        ps[2u*i]=hex[observed.shader.sha[i]>>4u];
        ps[2u*i+1u]=hex[observed.shader.sha[i]&15u];
    }
    char message[512]{};
    std::snprintf(message,sizeof(message),
       "[DSRRL V314 UNKNOWN PS RECEIVER] cut=NO_AUTHORIZED_OPERATOR"
       " source_family=%u (1=Lit,2=Mul,3=MulLit,0=UNKNOWN)"
       " source_exact=%u model_owner=%u current_parser=%u"
       " source_envslot=%u runtime_material_slot=%u"
       " stock_t1=1 ps_sha=%s ps_bytes=%u"
       " mtd=%02x%02x%02x%02x flver=%02x%02x%02x%02x"
       " authorization=DENIED pixel=OPEN",
       family,exact?1u:0u,m.owner_tuple_exact?1u:0u,
       m.actual_material_exact?1u:0u,slot,m.material_slot,ps,
       observed.shader.byte_size,
       m.raw_mtd_sha256[0],m.raw_mtd_sha256[1],
       m.raw_mtd_sha256[2],m.raw_mtd_sha256[3],
       m.flver_sha256[0],m.flver_sha256[1],
       m.flver_sha256[2],m.flver_sha256[3]);
    reshade::log::message(reshade::log::level::info,message);
}
} // namespace dsrrl::runtime::v314
