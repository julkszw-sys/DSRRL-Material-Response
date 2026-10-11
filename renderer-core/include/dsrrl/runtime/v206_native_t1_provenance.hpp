#pragma once
// V206: V204-native, bounded read-only CPU writer <-> D3D11 SRV provenance.
// Not a sidecar permission; no shader/material routes or global LightBank.
// In particular legacy SPC25 NPC c5330_s probes are intentionally absent.
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <mutex>

namespace dsrrl::runtime::v206_native_t1 {

struct witness {
    std::uint64_t epoch = 0;
    std::uint32_t source_id = 0;
    std::array<char,65u> logical_name{};
    explicit operator bool() const noexcept { return epoch != 0u; }
};

// Observation-only diagnostics: these values never authorize a replacement.
enum class probe_status : std::uint8_t {
    matched=0, view_absent, resource_mismatch, view_ambiguous,
    writer_absent, writer_conflict
};
struct probe_result {
    witness source{};
    probe_status status=probe_status::view_absent;
};
struct registry_stats {
    std::uint64_t init_called=0, init_accepted=0, writer_accepted=0;
};

template<std::size_t Capacity=4096u, std::size_t Writers=1024u>
class registry final {
    static_assert(Capacity>0u && (Capacity&(Capacity-1u))==0u);
    static constexpr std::size_t probe_limit=Capacity<16u?Capacity:16u;
    struct view_record {
        std::uintptr_t device=0u,view=0u,resource=0u;
        std::uint64_t epoch=0u;
        bool active=false,ambiguous=false;
    };
    struct writer_record {
        std::uintptr_t view=0u,resource=0u;
        std::uint64_t epoch=0u;
        std::uint32_t source_id=0u;
        std::array<char,65u> name{};
        bool active=false;
    };
    std::mutex mutex_;
    std::atomic<std::uint64_t> init_called_{0u}, init_accepted_{0u};
    std::atomic<std::uint64_t> writer_accepted_{0u};
    std::array<view_record,Capacity> views_{};
    std::array<writer_record,Writers> writers_{};
    std::uint64_t next_epoch_=0u;
    std::size_t writer_count_=0u;
    static constexpr std::size_t bucket(std::uintptr_t v) noexcept {
        return static_cast<std::size_t>(
            ((v>>4u)^(v>>17u)^(v>>29u))&(Capacity-1u));
    }
    view_record *current_unlocked(
        std::uintptr_t view,std::uintptr_t resource) noexcept {
        view_record *result=nullptr;
        for(std::size_t i=0;i<probe_limit;++i){
            auto &e=views_[(bucket(view)+i)&(Capacity-1u)];
            if(!e.active || e.view!=view)continue;
            if(e.ambiguous || e.resource!=resource || result)return nullptr;
            result=&e;
        }
        return result;
    }
    static bool equipment_spec_name(const char *s,std::size_t n) noexcept {
        if(n<6u||n>64u||s[n-2u]!='_'||
           (s[n-1u]!='s'&&s[n-1u]!='S')||s[2u]!='_')
            return false;
        auto lower=[](char c) noexcept {return c>='A'&&c<='Z'?char(c+('a'-'A')):c;};
        const auto a=lower(s[0]),b=lower(s[1]);
        if(!((a=='b'&&b=='d')||(a=='l'&&b=='g')||
             (a=='a'&&b=='m')||(a=='h'&&b=='d')||
             (a=='w'&&b=='p')))return false;
        for(std::size_t i=0;i<n;++i){
            const auto c=lower(s[i]);
            if(!((c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_'))
                return false;
        }
        return true;
    }
public:
    static bool name_is_equipment_spec(
        const char *name, std::size_t length) noexcept {
        return name && equipment_spec_name(name,length);
    }
    registry_stats stats() const noexcept {
        return {init_called_.load(std::memory_order_relaxed),
                init_accepted_.load(std::memory_order_relaxed),
                writer_accepted_.load(std::memory_order_relaxed)};
    }
    // Called on the V204 ReShade creation callback for EVERY stock D3D11 SRV,
    // not just already-named sidecar resources. Ambiguity poisons live view.
    std::uint64_t init(std::uintptr_t device,std::uintptr_t view,
                       std::uintptr_t resource) noexcept {
        init_called_.fetch_add(1u,std::memory_order_relaxed);
        if(!device||!view||!resource)return 0u;
        std::lock_guard<std::mutex> lock(mutex_);
        const auto start=bucket(view);
        std::size_t empty=Capacity;
        bool duplicate=false;
        for(std::size_t i=0;i<probe_limit;++i){
            auto &e=views_[(start+i)&(Capacity-1u)];
            if(e.active && e.view==view){e.ambiguous=true;duplicate=true;}
            else if(!e.active && empty==Capacity)empty=(start+i)&(Capacity-1u);
        }
        if(duplicate||empty==Capacity||
            next_epoch_==std::numeric_limits<std::uint64_t>::max())
            return 0u;
        views_[empty]={device,view,resource,++next_epoch_,true,false};
        init_accepted_.fetch_add(1u,std::memory_order_relaxed);
        return next_epoch_;
    }
    // Publish only exact engine packet + cache-name + verified DLTexture2D
    // observations; no pointer deref, COM ownership, or asset loads here.
    bool observe_writer(std::uintptr_t view,std::uintptr_t resource,
                        const char *logical_name,std::size_t length,
                        std::uint32_t source_id) noexcept {
        if(!view||!resource||!logical_name||!source_id||
           !equipment_spec_name(logical_name,length))return false;
        std::lock_guard<std::mutex> lock(mutex_);
        auto *e=current_unlocked(view,resource);
        if(!e||writer_count_==Writers)return false;
        // Never reuse a writer slot within the run. Retirement removes its
        // authority, preventing stale pointer address reuse.
        auto &w=writers_[writer_count_++];
        w.view=view;w.resource=resource;w.epoch=e->epoch;
        w.source_id=source_id;
        std::memcpy(w.name.data(),logical_name,length);
        w.name[length]=0;
        w.active=true;
        writer_accepted_.fetch_add(1u,std::memory_order_relaxed);
        return true;
    }
    // Inspect the live view independently of whether a writer was accepted.
    // Explicitly distinguishes missed lifecycle callbacks from missed names.
    probe_result probe(std::uintptr_t view,
                       std::uintptr_t resource) noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        view_record *live=nullptr;
        bool saw_view=false;
        for(std::size_t i=0;i<probe_limit;++i) {
            auto &e=views_[(bucket(view)+i)&(Capacity-1u)];
            if(!e.active || e.view!=view)continue;
            saw_view=true;
            if(e.ambiguous || live)
                return {{},probe_status::view_ambiguous};
            if(e.resource!=resource)
                return {{},probe_status::resource_mismatch};
            live=&e;
        }
        if(!live)
            return {{},saw_view ? probe_status::resource_mismatch :
                                probe_status::view_absent};
        witness found{};
        for(std::size_t i=0;i<writer_count_;++i){
            const auto &w=writers_[i];
            if(!w.active||w.view!=view)continue;
            if(w.resource!=resource||w.epoch!=live->epoch||found.epoch)
                return {{},probe_status::writer_conflict};
            found.epoch=w.epoch;found.source_id=w.source_id;
            found.logical_name=w.name;
        }
        return {found,found ? probe_status::matched :
                            probe_status::writer_absent};
    }
    witness resolve(std::uintptr_t view,
                    std::uintptr_t resource) noexcept {
        return probe(view,resource).source;
    }
    void destroy(std::uintptr_t device,std::uintptr_t view) noexcept {
        if(!device||!view)return;
        std::lock_guard<std::mutex> lock(mutex_);
        for(auto &e:views_)
            if(e.active&&e.device==device&&e.view==view)e.active=false;
        for(std::size_t i=0;i<writer_count_;++i)
            if(writers_[i].view==view)writers_[i].active=false;
    }
    void destroy_device(std::uintptr_t device) noexcept {
        if(!device)return;
        std::lock_guard<std::mutex> lock(mutex_);
        for(auto &e:views_)if(e.active&&e.device==device){
            for(std::size_t i=0;i<writer_count_;++i)
                if(writers_[i].view==e.view)writers_[i].active=false;
            e.active=false;
        }
    }
};

inline registry<> &instance() noexcept {
    static registry<> r;
    return r;
}
} // namespace dsrrl::runtime::v206_native_t1
