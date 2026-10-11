#pragma once
// V216: Native D3D11 resource equivalence for alternate *views* of the same
// proven stock texture. This is an identity carrier, NEVER a shader consumer.
//
// Insert only after the original view has (1) an exact original TLS name,
// (2) a live sidecar companion, (3) GetResource()==ReShade source_resource,
// and (4) that name is a member of the pinned 1295 Specular corpus.
//
// Inbound draw lookup uses GetResource() from *currently bound* t1. It may
// resolve an alternate SRV only via the same live native ID3D11Resource,
// device, stable logical hash and a still-registered owner view. No texture
// pointer prediction, FLVER-only inference, name-prefix matching or cache
// lifetime extrapolation. Conflicts quarantine the resource until destruction.
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <unordered_map>

namespace dsrrl::runtime::v216_native_resource_alias {

struct anchor {
    std::uintptr_t device=0;
    std::uintptr_t resource=0;
    std::uintptr_t view=0;
    std::uint64_t logical_hash=0;
    bool ambiguous=false;
};

class registry final {
    std::mutex mutex_;
    std::unordered_map<std::uintptr_t,anchor> by_resource_;
    std::unordered_map<std::uintptr_t,std::uintptr_t> by_view_;
    static constexpr std::size_t k_max=8192u;
public:
    bool observe(std::uintptr_t device,std::uintptr_t resource,
                 std::uintptr_t view,std::uint64_t logical_hash) noexcept {
        if (!device || !resource || !view || !logical_hash) return false;
        std::lock_guard<std::mutex> g(mutex_);
        const auto used=by_view_.find(view);
        if (used!=by_view_.end() && used->second!=resource) {
            const auto old=by_resource_.find(used->second);
            if (old!=by_resource_.end()) old->second.ambiguous=true;
            return false;
        }
        auto prior=by_resource_.find(resource);
        if (prior!=by_resource_.end()) {
            auto &entry=prior->second;
            if (entry.ambiguous) return false;
            if (entry.device!=device || entry.logical_hash!=logical_hash) {
                entry.ambiguous=true;
                return false;
            }
            // Keep the first live view as the anchor; other same-resource
            // views are eligible for recovery but cannot silently replace it.
            if (used==by_view_.end()) {
                try {by_view_.emplace(view,resource);}
                catch (...) {return false;}
            }
            return true;
        }
        if (by_resource_.size()>=k_max || by_view_.size()>=k_max)
            return false;
        try {
            by_resource_.emplace(resource,anchor{device,resource,view,logical_hash,false});
            try {by_view_.emplace(view,resource);}
            catch (...) {by_resource_.erase(resource);return false;}
        } catch (...) {return false;}
        return true;
    }

    bool resolve(std::uintptr_t device,std::uintptr_t resource,
                 anchor &out) noexcept {
        out={};
        if(!device || !resource) return false;
        std::lock_guard<std::mutex> g(mutex_);
        const auto i=by_resource_.find(resource);
        if (i==by_resource_.end() || i->second.ambiguous ||
            i->second.device!=device || !i->second.view)
            return false;
        const auto v=by_view_.find(i->second.view);
        if (v==by_view_.end() || v->second!=resource)
            return false;
        out=i->second;
        return true;
    }

    void destroy_view(std::uintptr_t view) noexcept {
        if(!view) return;
        std::lock_guard<std::mutex> g(mutex_);
        const auto i=by_view_.find(view);
        if(i==by_view_.end()) return;
        const auto res=i->second;
        by_view_.erase(i);
        const auto a=by_resource_.find(res);
        if(a!=by_resource_.end() && a->second.view==view) {
            // Anchor has been destroyed: do not guess a surviving owner.
            by_resource_.erase(a);
        }
    }

    void destroy_resource(std::uintptr_t device,std::uintptr_t resource) noexcept {
        if(!device || !resource) return;
        std::lock_guard<std::mutex> g(mutex_);
        auto it=by_resource_.find(resource);
        if(it==by_resource_.end() || it->second.device!=device) return;
        for(auto v=by_view_.begin();v!=by_view_.end();) {
            if(v->second==resource)v=by_view_.erase(v);
            else ++v;
        }
        by_resource_.erase(it);
    }

    void destroy_device(std::uintptr_t device) noexcept {
        if(!device) return;
        std::lock_guard<std::mutex> g(mutex_);
        for(auto it=by_resource_.begin();it!=by_resource_.end();) {
            if(it->second.device!=device) {++it;continue;}
            for(auto v=by_view_.begin();v!=by_view_.end();) {
                if(v->second==it->first)v=by_view_.erase(v);
                else ++v;
            }
            it=by_resource_.erase(it);
        }
    }
};

inline registry &instance() noexcept {
    static registry state;
    return state;
}
} // namespace dsrrl::runtime::v216_native_resource_alias
