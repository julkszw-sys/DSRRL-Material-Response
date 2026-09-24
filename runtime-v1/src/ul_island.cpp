#include "dsrrl/runtime/ul_island.hpp"

#include "dsrrl/core/renderer_core.hpp"
#include "dsrrl/runtime/engine_hooks.hpp"

#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <unordered_map>

namespace dsrrl::runtime::ul {
namespace {

constexpr std::uintptr_t k_ret_blend_upper = 0x5639BA;
constexpr std::uintptr_t k_ret_blend_lower = 0x5639D5;
constexpr std::uintptr_t k_ret_sel_1 = 0x20E019;
constexpr std::uintptr_t k_ret_sel_2 = 0x20EB7F;
constexpr std::uintptr_t k_ret_sel_3 = 0x20FB9E;

constexpr float k_inv_gamma = 1.0f / 2.2f;

#pragma pack(push,1)
struct raw_rgbm {
    std::int16_t r = 0;
    std::int16_t g = 0;
    std::int16_t b = 0;
    std::int16_t m = 0;
};
#pragma pack(pop)

struct producer_state {
    bool active = false;
    std::uintptr_t owner = 0;
    const std::uint8_t *assignment = nullptr;
    bool have_upper = false;
    bool have_lower = false;
    core::float4 upper{};
    core::float4 lower{};
};

core::renderer_core *g_core = nullptr;
std::mutex g_snapshot_mutex;
std::unordered_map<std::uintptr_t,std::shared_ptr<const draw_snapshot>> g_snapshots;
thread_local std::shared_ptr<const draw_snapshot> g_draw_snapshot;

thread_local producer_state g_producer;
thread_local std::array<producer_state,4> g_producer_stack{};
thread_local std::uint32_t g_producer_depth = 0;

std::atomic<std::uint64_t> g_wrapper_enter{0};
std::atomic<std::uint64_t> g_steady_pass{0},g_steady_fail{0};
std::atomic<std::uint64_t> g_blend_upper{0},g_blend_lower{0};
std::atomic<std::uint64_t> g_snapshots_published{0};
std::atomic<std::uint64_t> g_selector_match{0},g_selector_fail{0};

template<class T>
bool safe_read(const void *at,T &value) noexcept
{
    return at && engine::safe_read_bytes(at,&value,sizeof(value));
}

core::float4 decode_rgbm(const raw_rgbm &v) noexcept
{
    const float scale = (static_cast<float>(v.m) / 100.0f) / 255.0f;
    return {
        static_cast<float>(v.r) * scale,
        static_cast<float>(v.g) * scale,
        static_cast<float>(v.b) * scale,
        0.0f
    };
}

core::float4 lerp4(
    const core::float4 &a,const core::float4 &b,float beta) noexcept
{
    const float inv = 1.0f - beta;
    return {
        inv*a.x + beta*b.x,
        inv*a.y + beta*b.y,
        inv*a.z + beta*b.z,
        0.0f
    };
}

bool decode_cached_q(const float q[4],core::float4 &out) noexcept
{
    const float x[3] = {q[0],q[1],q[2]};
    float p[3]{};
    for(std::size_t i=0;i<3;++i){
        if(!std::isfinite(x[i]) || x[i] < 0.0f)
            return false;
        p[i] = std::pow(x[i],k_inv_gamma);
        if(!std::isfinite(p[i]))
            return false;
    }
    out = {p[0],p[1],p[2],0.0f};
    return true;
}

void publish_snapshot(const producer_state &p) noexcept
{
    if(!p.active || !p.owner || !p.assignment ||
       !p.have_upper || !p.have_lower)
        return;

    std::uint16_t a=0,b=0;
    std::uint32_t beta=0;
    if(!safe_read(p.assignment+8,a) ||
       !safe_read(p.assignment+10,b) ||
       !safe_read(p.assignment+12,beta))
        return;

    auto snapshot = std::make_shared<draw_snapshot>();
    snapshot->owner = p.owner;
    snapshot->area_a = a;
    snapshot->area_b = b;
    snapshot->beta_bits = beta;
    snapshot->carrier[6] = p.upper;
    snapshot->carrier[7] = p.lower;

    {
        std::lock_guard lock(g_snapshot_mutex);
        g_snapshots[p.owner] = snapshot;
    }
    ++g_snapshots_published;
}

void wrapper_enter(void *owner,void *assignment) noexcept
{
    ++g_wrapper_enter;
    if(g_producer_depth >= g_producer_stack.size()){
        g_producer = {};
        return;
    }

    g_producer_stack[g_producer_depth++] = g_producer;
    g_producer = {
        true,
        reinterpret_cast<std::uintptr_t>(owner),
        static_cast<const std::uint8_t *>(assignment),
        false,false,{},{}
    };
}

void wrapper_exit() noexcept
{
    if(g_producer_depth == 0){
        g_producer = {};
        return;
    }

    const producer_state completed = g_producer;
    g_producer = g_producer_stack[--g_producer_depth];
    publish_snapshot(completed);
}

void steady_cache(void *source,std::int32_t selector) noexcept
{
    if(!g_core ||
       !g_core->features().enabled(core::operator_id::upper_lower) ||
       !g_producer.active || !source){
        return;
    }

    if(selector < 0){
        ++g_steady_fail;
        return;
    }

    const auto *bytes = static_cast<const std::uint8_t *>(source);
    void *meta = nullptr;
    std::uint16_t count = 0;
    void *records = nullptr;

    if(!safe_read(bytes+0x18,meta) || !meta ||
       !safe_read(static_cast<const std::uint8_t *>(meta)+0x0A,count) ||
       !safe_read(bytes+0x20,records) || !records){
        ++g_steady_fail;
        return;
    }

    const std::uint32_t selected =
        static_cast<std::uint32_t>(static_cast<std::uint8_t>(selector));
    if(selected >= count){
        ++g_steady_fail;
        return;
    }

    const auto *record =
        static_cast<const std::uint8_t *>(records) +
        static_cast<std::size_t>(selected) * 0x110u;

    float upper_q[4]{},lower_q[4]{};
    if(!engine::safe_read_bytes(record+0x60,upper_q,sizeof(upper_q)) ||
       !engine::safe_read_bytes(record+0x70,lower_q,sizeof(lower_q))){
        ++g_steady_fail;
        return;
    }

    core::float4 upper{},lower{};
    if(!decode_cached_q(upper_q,upper) ||
       !decode_cached_q(lower_q,lower)){
        ++g_steady_fail;
        return;
    }

    g_producer.upper = upper;
    g_producer.lower = lower;
    g_producer.have_upper = true;
    g_producer.have_lower = true;
    ++g_steady_pass;
}

void true_blend(
    const void *a,const void *b,float beta,std::uintptr_t return_rva) noexcept
{
    if(!g_core ||
       !g_core->features().enabled(core::operator_id::upper_lower) ||
       !g_producer.active || !a || !b || !std::isfinite(beta))
        return;

    raw_rgbm ra{},rb{};
    if(!safe_read(a,ra) || !safe_read(b,rb))
        return;

    const auto value = lerp4(decode_rgbm(ra),decode_rgbm(rb),beta);
    if(return_rva == k_ret_blend_upper){
        g_producer.upper = value;
        g_producer.have_upper = true;
        ++g_blend_upper;
    }else if(return_rva == k_ret_blend_lower){
        g_producer.lower = value;
        g_producer.have_lower = true;
        ++g_blend_lower;
    }
}

} // namespace

bool register_runtime(core::renderer_core &core) noexcept
{
    g_core = &core;
    return true;
}

void unregister_runtime() noexcept
{
    clear_draw_snapshot();
    {
        std::lock_guard lock(g_snapshot_mutex);
        g_snapshots.clear();
    }
    g_producer = {};
    g_producer_depth = 0;
    g_core = nullptr;
}

engine::upper_lower_callbacks callbacks() noexcept
{
    return {
        &wrapper_enter,
        &wrapper_exit,
        &steady_cache,
        &true_blend
    };
}

void selector_event(
    void *owner,void *ret,void *r14,void *r15) noexcept
{
    if(!g_core ||
       !g_core->features().enabled(core::operator_id::upper_lower)){
        clear_draw_snapshot();
        return;
    }

    const auto base = engine::image_base();
    if(!base){
        ++g_selector_fail;
        clear_draw_snapshot();
        return;
    }

    const auto rva =
        reinterpret_cast<std::uintptr_t>(ret) - base;
    const std::uint8_t *desc = nullptr;
    if(rva == k_ret_sel_1 || rva == k_ret_sel_3)
        desc = static_cast<const std::uint8_t *>(r15);
    else if(rva == k_ret_sel_2)
        desc = static_cast<const std::uint8_t *>(r14);
    else{
        ++g_selector_fail;
        clear_draw_snapshot();
        return;
    }

    if(!owner || !desc){
        ++g_selector_fail;
        clear_draw_snapshot();
        return;
    }

    std::uint16_t a=0,b=0;
    std::uint32_t beta=0;
    if(!safe_read(desc+0x4C,a) ||
       !safe_read(desc+0x4E,b) ||
       !safe_read(desc+0x50,beta)){
        ++g_selector_fail;
        clear_draw_snapshot();
        return;
    }

    std::shared_ptr<const draw_snapshot> snapshot;
    {
        std::lock_guard lock(g_snapshot_mutex);
        const auto it =
            g_snapshots.find(reinterpret_cast<std::uintptr_t>(owner));
        if(it != g_snapshots.end())
            snapshot = it->second;
    }

    if(!snapshot ||
       snapshot->area_a != a ||
       snapshot->area_b != b ||
       snapshot->beta_bits != beta){
        ++g_selector_fail;
        clear_draw_snapshot();
        return;
    }

    g_draw_snapshot = std::move(snapshot);
    ++g_selector_match;
}

std::shared_ptr<const draw_snapshot> consume_draw_snapshot() noexcept
{
    auto result = std::move(g_draw_snapshot);
    g_draw_snapshot.reset();
    return result;
}

void clear_draw_snapshot() noexcept
{
    g_draw_snapshot.reset();
}

bool native_draw_matches(
    const draw_snapshot &snapshot,
    std::uint32_t instance_count,
    std::uint32_t first_index,
    std::int32_t vertex_offset,
    std::uint32_t first_instance,
    std::uint32_t &native_instances) noexcept
{
    native_instances = 0;
    if(first_index != 0 || vertex_offset != 0 || first_instance != 0 ||
       snapshot.owner == 0)
        return false;

    if(!safe_read(
        reinterpret_cast<const void *>(snapshot.owner + 0x24BC),
        native_instances))
        return false;

    if(native_instances != 0)
        return instance_count == native_instances;
    return instance_count == 1;
}

} // namespace dsrrl::runtime::ul
