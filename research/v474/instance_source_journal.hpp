#pragma once
// V474: source-domain *candidate* for a future RX34 selector island.
// Not included in v2.3.1 or any renderer target. No native pointers dereferenced.
// Caller must authenticate original native FrpgModelDrawEntity vtable,
// exact FrpgDrawParam offset, per-instance native builder emission/issued ticket.
// A model's FLVER/MTD alone is NEVER owner authority.
#include <array>
#include <cstdint>
#include <cstddef>
#include <mutex>
#include <utility>
#include "dsrrl/runtime/pmetal_env_source_runtime.hpp"

namespace dsrrl::research::v474 {
using material_identity=operators::material_response::material_identity;
using source=runtime::pmetal_envspec_source;

struct exact_ticket {
    std::uintptr_t entity=0u;
    std::uintptr_t draw_param=0u;
    std::uint32_t full_handle=0u;
    std::uint64_t native_builder_sequence=0u;
    std::uint64_t selector_epoch=0u;
    std::uint32_t receiver=0u;
    material_identity material{};
    // These are verdicts from independently authenticated native receiver
    // and issuer callsites, not assertions inferred from the material/row.
    bool typed_model_entity=false;
    bool native_source1_issued=false;
};

inline bool same_material(const material_identity &a,const material_identity &b) noexcept {
    return a.valid==b.valid &&
      a.owner_tuple_exact==b.owner_tuple_exact &&
      a.material_slot_valid==b.material_slot_valid &&
      a.actual_material_exact==b.actual_material_exact &&
      a.flver_sha256==b.flver_sha256 &&
      a.raw_mtd_sha256==b.raw_mtd_sha256 &&
      a.material_slot==b.material_slot &&
      a.route_index==b.route_index &&
      a.semantic_name_hash==b.semantic_name_hash &&
      a.material_family_hash==b.material_family_hash;
}
inline bool authentic(const exact_ticket &t) noexcept {
    return t.entity!=0 && t.draw_param>=t.entity &&
      t.draw_param-t.entity==0x108u &&
      t.full_handle!=0u &&
      t.native_builder_sequence!=0u &&
      t.selector_epoch!=0u &&
      t.receiver==34u &&
      t.typed_model_entity && t.native_source1_issued &&
      t.material.valid && t.material.owner_tuple_exact &&
      t.material.material_slot_valid && t.material.actual_material_exact &&
      t.material.route_index==345u;
}
inline bool identical(const exact_ticket &a,const exact_ticket &b) noexcept {
    return a.entity==b.entity && a.draw_param==b.draw_param &&
      a.full_handle==b.full_handle &&
      a.native_builder_sequence==b.native_builder_sequence &&
      a.selector_epoch==b.selector_epoch &&
      a.receiver==b.receiver && same_material(a.material,b.material);
}
inline bool same_registration(const exact_ticket &a,const exact_ticket &b) noexcept {
    return a.selector_epoch==b.selector_epoch &&
      a.full_handle==b.full_handle;
}
inline bool same_emission(const source &a,const source &b) noexcept {
    return a.a==b.a && a.b==b.b && a.beta==b.beta &&
      a.row_id_a==b.row_id_a && a.row_id_b==b.row_id_b &&
      a.bank_signature_a==b.bank_signature_a &&
      a.bank_signature_b==b.bank_signature_b &&
      a.envdiffuse_linear_valid==b.envdiffuse_linear_valid &&
      a.envdiffuse_a==b.envdiffuse_a &&
      a.envdiffuse_b==b.envdiffuse_b;
}
class exact_instance_journal {
    struct entry {
        exact_ticket t{};
        source s{};
        bool valid=false;
        bool poisoned=false;
    };
    std::array<entry,128> slots_{};
    std::mutex mutex_;
public:
    // Issuer-observed native DSR Source1 only. No synthetic row conversion.
    bool publish(const exact_ticket &t,const source &s) noexcept {
        if(!authentic(t))return false;
        std::lock_guard<std::mutex> guard(mutex_);
        // Colliding full handle in same epoch is an identity fault, not
        // an invitation to choose "last source".
        bool conflict=false;
        for(auto &x:slots_)if(x.valid && same_registration(x.t,t)) {
            if(x.t.entity!=t.entity || x.t.draw_param!=t.draw_param) {
                x.poisoned=true;conflict=true;
            }
        }
        if(conflict)return false;
        for(auto &x:slots_)if(x.valid && identical(x.t,t)) {
            if(!same_emission(x.s,s)) {x.poisoned=true;return false;}
            return !x.poisoned;
        }
        for(auto &x:slots_)if(!x.valid) {
            x.t=t;x.s=s;x.valid=true;x.poisoned=false;return true;
        }
        return false; // overflow: fail-open stock
    }
    // Issued RX34 must supply the *same native builder ticket* and handle.
    // A material-only, pointer-only, low-byte-only or latest-row query fails.
    bool read(const exact_ticket &t,source &out) noexcept {
        out={};
        if(!authentic(t))return false;
        std::lock_guard<std::mutex> guard(mutex_);
        for(const auto &x:slots_)if(x.valid && same_registration(x.t,t) &&
          (x.t.entity!=t.entity || x.t.draw_param!=t.draw_param))
            return false; // registered-handle collision (ABA)
        for(const auto &x:slots_)if(x.valid && identical(x.t,t)) {
            if(x.poisoned)return false;
            out=x.s;return true;
        }
        return false;
    }
    void retire(std::uintptr_t entity,std::uint32_t handle) noexcept {
        std::lock_guard<std::mutex> guard(mutex_);
        for(auto &x:slots_)if(x.valid &&
            x.t.entity==entity && x.t.full_handle==handle) x=entry{};
    }
    void reset() noexcept {
        std::lock_guard<std::mutex> guard(mutex_);
        for(auto &x:slots_)x=entry{};
    }
};
} // namespace dsrrl::research::v474
