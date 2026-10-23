#!/usr/bin/env python3
"""V230 exact Source1 producer/consumer join evidence; read-only, fail-open."""
from pathlib import Path
p=Path("renderer-core/src/runtime/pmetal_producer_state.cpp")
s=p.read_text("utf-8")
def patch(old,new):
    global s
    assert s.count(old)==1, ("V230_SOURCE_JOIN_DRIFT",old[:70],s.count(old))
    s=s.replace(old,new,1)
patch("#include <mutex>","#include <mutex>\n#if defined(DSRRL_V230_SOURCE_JOIN_CAUSE)\n#include <cstdio>\n#include <reshade.hpp>\n#endif")
anchor="} // namespace\n\nvoid pmetal_producer_state_clear()"
helper=r"""#if defined(DSRRL_V230_SOURCE_JOIN_CAUSE)
// PHASE: 0 exact selector begin, 1 publish, 2 synchronized fast reject,
// 3 synchronized deep reject, 4 synchronized success, 5 same-thread success.
// All comparisons and existing producer/fallback behavior remain unchanged.
void trace_source_join(unsigned phase, unsigned reason,
    const operators::material_response::material_identity &m,
    std::uint64_t epoch, std::uint64_t key,
    const synchronized_slot &slot) noexcept
{
    if(phase>5u)return;
    static std::array<std::atomic<std::uint32_t>,6> counters{};
    const auto n=counters[phase].fetch_add(1u,std::memory_order_relaxed)+1u;
    if(n>45u && n!=128u && n!=512u && n!=2048u)return;
    char b[515]{};
    std::snprintf(b,sizeof(b),
        "[DSRRL SOURCE JOIN CAUSE] p=%u n=%u reason=%u route=%u slot=%u owner=%u actual=%u "
        "flver8=%02x%02x%02x%02x mtd8=%02x%02x%02x%02x "
        "epoch=%llu key=%016llx sync_valid=%u sync_epoch=%llu sync_key=%016llx "
        "tls_valid=%u tls_epoch=%llu",
        phase,n,reason,m.route_index,m.material_slot,m.owner_tuple_exact?1u:0u,
        m.actual_material_exact?1u:0u,
        m.flver_sha256[0],m.flver_sha256[1],m.flver_sha256[2],m.flver_sha256[3],
        m.raw_mtd_sha256[0],m.raw_mtd_sha256[1],m.raw_mtd_sha256[2],m.raw_mtd_sha256[3],
        (unsigned long long)epoch,(unsigned long long)key,
        slot.valid.load(std::memory_order_relaxed)?1u:0u,
        (unsigned long long)slot.epoch.load(std::memory_order_relaxed),
        (unsigned long long)slot.material_key.load(std::memory_order_relaxed),
        g_record.valid?1u:0u,(unsigned long long)g_record.epoch);
    reshade::log::message(reshade::log::level::warning,b);
}
#endif
"""
patch(anchor,helper+anchor)
patch("""    slot.epoch.store(
        epoch,
        std::memory_order_relaxed);
}

void pmetal_producer_state_publish(""","""    slot.epoch.store(
        epoch,
        std::memory_order_relaxed);
#if defined(DSRRL_V230_SOURCE_JOIN_CAUSE)
    trace_source_join(0u,0u,material,epoch,key,slot);
#endif
}

void pmetal_producer_state_publish(""")
patch("""    auto &slot = sync_slot_for(key);

    // Same exact material/source/epoch:""","""    auto &slot = sync_slot_for(key);
#if defined(DSRRL_V230_SOURCE_JOIN_CAUSE)
    trace_source_join(1u,0u,material,epoch,key,slot);
#endif

    // Same exact material/source/epoch:""")
patch("""        out = g_record.source;
        return true;
    }

    // Exact synchronized fallback""","""        out = g_record.source;
#if defined(DSRRL_V230_SOURCE_JOIN_CAUSE)
        const auto key=material_key(material);
        trace_source_join(5u,0u,material,epoch,key,sync_slot_for(key));
#endif
        return true;
    }

    // Exact synchronized fallback""")
patch("""        slot.epoch.load(
            std::memory_order_relaxed) != epoch)
        return false;

    std::lock_guard<std::mutex> lock(slot.mutex);""","""        slot.epoch.load(
            std::memory_order_relaxed) != epoch) {
#if defined(DSRRL_V230_SOURCE_JOIN_CAUSE)
        const unsigned why = !slot.valid.load(std::memory_order_relaxed)?1u:
            slot.material_key.load(std::memory_order_relaxed)!=key?2u:3u;
        trace_source_join(2u,why,material,epoch,key,slot);
#endif
        return false;
    }

    std::lock_guard<std::mutex> lock(slot.mutex);""")
patch("""        !same_material(
            slot.record.material,
            material))
        return false;

    out = slot.record.source;""","""        !same_material(
            slot.record.material,
            material)) {
#if defined(DSRRL_V230_SOURCE_JOIN_CAUSE)
        const unsigned why = !slot.record.valid?4u:
            slot.record.epoch!=epoch?5u:6u;
        trace_source_join(3u,why,material,epoch,key,slot);
#endif
        return false;
    }

    out = slot.record.source;
#if defined(DSRRL_V230_SOURCE_JOIN_CAUSE)
    trace_source_join(4u,0u,material,epoch,key,slot);
#endif""")
p.write_text(s,encoding="utf-8")
print("SOURCE_JOIN_CAUSE_PASS phase-coded selector begin/publish/consumer; fallback unchanged")
