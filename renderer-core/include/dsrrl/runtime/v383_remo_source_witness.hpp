#pragma once
// V383 READ ONLY. Observe authenticated material-side selector and native LightBank source.
// Not a Type180 event reader, model +0xDE reader or producer-to-actor proof.
#include <array>
#include <atomic>
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <reshade.hpp>
#include "dsrrl/operators/material_response/material_response_island.hpp"
namespace dsrrl::runtime {
inline void v383_model_material_selector(
    void *owner,const void *descriptor,
    const operators::material_response::material_identity &m,
    std::int16_t a,std::int16_t b,float beta,unsigned character,unsigned tid) noexcept
{
#if defined(DSRRL_V230_RUNTIME_TELEMETRY)
    if(!owner||!descriptor||!m.valid||!m.owner_tuple_exact||!m.material_slot_valid)return;
    std::uint64_t sha_head=0u; std::memcpy(&sha_head,m.flver_sha256.data(),sizeof(sha_head));
    std::uint32_t beta_bits=0u;std::memcpy(&beta_bits,&beta,sizeof(beta_bits));
    std::uint64_t key=
        (static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(owner))>>3u) ^
        (sha_head*0x9e3779b97f4a7c15ull) ^
        (m.semantic_name_hash*0x517cc1b727220a95ull) ^
        (static_cast<std::uint64_t>(m.route_index)<<19u) ^
        (static_cast<std::uint64_t>(m.material_slot)<<35u) ^
        (static_cast<std::uint64_t>(static_cast<std::uint16_t>(a))<<11u) ^
        (static_cast<std::uint64_t>(static_cast<std::uint16_t>(b))<<47u) ^
        (static_cast<std::uint64_t>(beta_bits)<<3u)^character;
    key|=1ull;
    static std::array<std::atomic<std::uint64_t>,4096u> seen{};
    bool first=false;
    for(unsigned i=0;i<12u;++i){
        auto &cell=seen[(key+i)%seen.size()];
        auto old=cell.load(std::memory_order_relaxed);
        if(old==key)return;
        if(old==0u){
            std::uint64_t zero=0u;
            if(cell.compare_exchange_strong(zero,key,std::memory_order_relaxed)){first=true;break;}
        }
    }
    if(!first)return;
    static std::atomic<unsigned> count{0u};
    const auto n=count.fetch_add(1u,std::memory_order_relaxed)+1u;
    if(n>4096u)return;
    char sha[65]{}; constexpr char hexdigits[]="0123456789abcdef";
    for(std::size_t i=0;i<32u;++i){
        sha[i*2u]=hexdigits[m.flver_sha256[i]>>4u];
        sha[i*2u+1u]=hexdigits[m.flver_sha256[i]&15u];
    }
    char line[690]{};
    std::snprintf(line,sizeof(line),
        "[DSRRL V383 REMO MODEL MATERIAL] n=%u owner=%p descriptor=%p tid=%u "
        "route=%u slot=%u semantic=%016llx flver_sha256=%s "
        "resolved_descriptor_a=%d resolved_descriptor_b=%d beta=%.9g character=%u "
        "TAE180_input=NOT_SAMPLED model_default_DE=NOT_SAMPLED pixel=OPEN",
        n,owner,descriptor,tid,m.route_index,m.material_slot,
        static_cast<unsigned long long>(m.semantic_name_hash),sha,
        static_cast<int>(a),static_cast<int>(b),static_cast<double>(beta),character);
    reshade::log::message(reshade::log::level::warning,line);
#else
    (void)owner;(void)descriptor;(void)m;(void)a;(void)b;(void)beta;(void)character;(void)tid;
#endif
}
inline void v383_native_lightbank_producer(
    const void *source_a,const void *source_b,
    std::int16_t selector_a,std::int16_t selector_b,
    std::uint32_t row_a,std::uint32_t row_b,
    std::uint64_t serial,unsigned tid) noexcept
{
#if defined(DSRRL_V230_RUNTIME_TELEMETRY)
    std::uint64_t key=
        (static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(source_a))>>3u) ^
        (static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(source_b))<<7u) ^
        (static_cast<std::uint64_t>(static_cast<std::uint16_t>(selector_a))<<13u) ^
        (static_cast<std::uint64_t>(static_cast<std::uint16_t>(selector_b))<<29u) ^
        (static_cast<std::uint64_t>(row_a)<<19u) ^
        (static_cast<std::uint64_t>(row_b)<<43u);
    key|=1ull;
    thread_local std::uint64_t previous=0u;
    if(previous==key)return;previous=key;
    const bool focused=row_a==0u||row_a==5u||row_b==0u||row_b==5u;
    static std::atomic<unsigned> focus_count{0u},other_count{0u};
    const unsigned n=(focused?focus_count:other_count).fetch_add(1u,std::memory_order_relaxed)+1u;
    if(n>(focused?8192u:64u))return;
    char line[470]{};
    std::snprintf(line,sizeof(line),
        "[DSRRL V383 NATIVE LIGHTBANK] n=%u srcA=%p srcB=%p "
        "selectorA=%d selectorB=%d decoded_rowA=%u decoded_rowB=%u "
        "serial=%llu tid=%u model_join=OPEN material_join=OPEN pixel=OPEN",
        n,source_a,source_b,static_cast<int>(selector_a),static_cast<int>(selector_b),
        row_a,row_b,static_cast<unsigned long long>(serial),tid);
    reshade::log::message(reshade::log::level::warning,line);
#else
    (void)source_a;(void)source_b;(void)selector_a;(void)selector_b;
    (void)row_a;(void)row_b;(void)serial;(void)tid;
#endif
}
}
