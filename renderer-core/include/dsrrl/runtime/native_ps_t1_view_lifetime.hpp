#pragma once
// SPC25: non-owning native SRV lifetime census; no texture mutation.
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <mutex>
namespace dsrrl::runtime::native_ps_t1_lifetime {
struct observation {
    std::uint64_t epoch = 0;
    std::uintptr_t device = 0;
    explicit operator bool() const noexcept { return epoch != 0; }
};
template<std::size_t Capacity = 4096u> class registry final {
    struct entry {
        std::uintptr_t device = 0, view = 0, resource = 0;
        std::uint64_t epoch = 0;
        bool active = false, ambiguous = false;
    };
    std::mutex mutex_;
    std::array<entry, Capacity> slots_{};
    std::uint64_t next_epoch_ = 0;
public:
    std::uint64_t init(std::uintptr_t d,std::uintptr_t v,std::uintptr_t r) noexcept {
        if(!d || !v || !r) return 0;
        std::lock_guard<std::mutex> lock(mutex_);
        bool duplicate = false;
        for(auto &s: slots_) if(s.active && s.view==v) {s.ambiguous=true; duplicate=true;}
        if(duplicate || next_epoch_==std::numeric_limits<std::uint64_t>::max()) return 0;
        for(auto &s: slots_) if(!s.active) {
            s={d,v,r,++next_epoch_,true,false}; return s.epoch;
        }
        return 0; // Capacity exhaustion: no inference.
    }
    void destroy(std::uintptr_t d,std::uintptr_t v) noexcept {
        if(!d || !v) return;
        std::lock_guard<std::mutex> lock(mutex_);
        for(auto &s:slots_) if(s.active && s.device==d && s.view==v) s.active=false;
    }
    void destroy_device(std::uintptr_t d) noexcept {
        if(!d) return;
        std::lock_guard<std::mutex> lock(mutex_);
        for(auto &s:slots_) if(s.active && s.device==d) s.active=false;
    }
    observation current(std::uintptr_t v,std::uintptr_t r) noexcept {
        if(!v || !r) return {};
        std::lock_guard<std::mutex> lock(mutex_);
        observation found{};
        for(const auto &s:slots_) if(s.active && s.view==v) {
            if(s.ambiguous || s.resource!=r || found.epoch) return {};
            found={s.epoch,s.device};
        }
        return found;
    }
};
inline registry<> &instance() noexcept {
    static registry<> epochs;
    return epochs;
}
} // namespace dsrrl::runtime::native_ps_t1_lifetime
