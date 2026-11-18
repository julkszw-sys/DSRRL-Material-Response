#pragma once
// V475 Source1 only. Real material_response types, exact typed native
// FrpgModelDrawEntity/FrpgDrawParam ticket; never infer actor from FLVER.
// No static row rewrite, no DSR executable mutation, no R43 Source2 mutation.
// This is a same-native-builder-scope one-shot transport, not a gameplay owner ID.
#include <cstddef>
#include <cstdint>
#include "dsrrl/runtime/pmetal_env_source_runtime.hpp"

namespace dsrrl::runtime::v475 {
using material_identity = operators::material_response::material_identity;
using source = pmetal_envspec_source;

struct ticket {
    std::uintptr_t entity = 0u;
    std::uintptr_t draw_param = 0u;
    std::uintptr_t packet = 0u;
    std::uintptr_t context = 0u;
    std::uintptr_t engine_model = 0u;
    std::uintptr_t native_frame = 0u;
    std::uint32_t registration_handle = 0u; // FULL generational DWORD, NOT low bits
    std::uint16_t packed_a = 0u;
    std::uint16_t packed_b = 0u;
    std::uint32_t beta_bits = 0u;
    std::uint64_t selector_epoch = 0u;
    material_identity material{};
    bool exact_type = false;
    bool default_builder = false;
};

inline bool same_material(const material_identity &a,
                          const material_identity &b) noexcept {
    return a.valid == b.valid &&
        a.owner_tuple_exact == b.owner_tuple_exact &&
        a.material_slot_valid == b.material_slot_valid &&
        a.actual_material_exact == b.actual_material_exact &&
        a.flver_sha256 == b.flver_sha256 &&
        a.raw_mtd_sha256 == b.raw_mtd_sha256 &&
        a.material_slot == b.material_slot &&
        a.route_index == b.route_index &&
        a.semantic_name_hash == b.semantic_name_hash &&
        a.material_family_hash == b.material_family_hash;
}

inline bool same_source(const source &a, const source &b) noexcept {
    return a.a == b.a && a.b == b.b && a.beta == b.beta &&
        a.bank_signature_a == b.bank_signature_a &&
        a.bank_signature_b == b.bank_signature_b &&
        a.row_id_a == b.row_id_a && a.row_id_b == b.row_id_b &&
        a.envdiffuse_linear_valid == b.envdiffuse_linear_valid &&
        a.envdiffuse_a == b.envdiffuse_a &&
        a.envdiffuse_b == b.envdiffuse_b &&
        a.phn_k135 == b.phn_k135 &&
        a.phn_k135_valid == b.phn_k135_valid;
}

inline bool targeted_receiver(const material_identity &material,
                              std::uint32_t receiver_id) noexcept {
    return receiver_id == 34u && material.valid &&
        material.owner_tuple_exact && material.material_slot_valid &&
        material.actual_material_exact && material.route_index == 345u;
}

inline bool well_formed(const ticket &t) noexcept {
    if (!t.exact_type || !t.default_builder ||
        t.entity == 0u || t.entity > UINTPTR_MAX - 0x108u ||
        t.draw_param != t.entity + 0x108u ||
        t.packet == 0u || t.context == 0u ||
        t.engine_model == 0u || t.native_frame == 0u ||
        t.registration_handle == 0u ||
        t.registration_handle == UINT32_MAX ||
        t.selector_epoch == 0u ||
        !t.material.valid || !t.material.owner_tuple_exact ||
        !t.material.material_slot_valid ||
        !t.material.actual_material_exact ||
        t.material.route_index != 345u)
        return false;
    // First supported island is native steady row30/31 only.
    // Do not generalize to other materials, lerp, map or actors by guess.
    return t.packed_a == t.packed_b && t.beta_bits == 0u &&
        (t.packed_a == 0x051eu || t.packed_a == 0x051fu);
}

// The native FLVER selector descriptor is a different object from the model
// builder packet. Positive Source1 ingress requires equality at their actual
// semantic cut, not merely equal low-byte row or shared FLVER material.
inline bool same_native_descriptor(const ticket &t,
                                   std::uintptr_t descriptor_owner,
                                   std::uint16_t descriptor_a,
                                   std::uint16_t descriptor_b,
                                   std::uint32_t descriptor_beta_bits) noexcept {
    return well_formed(t) &&
        t.context == descriptor_owner &&
        t.packed_a == descriptor_a &&
        t.packed_b == descriptor_b &&
        t.beta_bits == descriptor_beta_bits;
}

inline bool source_matches_ticket(const ticket &t, const source &s) noexcept {
    if (!well_formed(t))
        return false;
    const auto row = static_cast<std::uint32_t>(t.packed_a & 0xffu);
    return s.beta == 0.0f &&
        s.row_id_a == row && s.row_id_b == row &&
        s.bank_signature_a != 0u &&
        s.bank_signature_a == s.bank_signature_b &&
        s.a == s.b;
}

inline bool same_scope(const ticket &a, const ticket &b) noexcept {
    return a.entity == b.entity &&
        a.draw_param == b.draw_param &&
        a.packet == b.packet &&
        a.context == b.context &&
        a.engine_model == b.engine_model &&
        a.native_frame == b.native_frame &&
        a.registration_handle == b.registration_handle &&
        a.packed_a == b.packed_a &&
        a.packed_b == b.packed_b &&
        a.beta_bits == b.beta_bits &&
        a.selector_epoch == b.selector_epoch &&
        same_material(a.material, b.material);
}

enum class verdict : std::uint8_t {
    absent = 0,   // no authenticated Source1 receipt; leave original behavior
    exact = 1,    // valid one-shot per-instance source
    mismatch = 2, // same exact material/epoch, different native instance -> stock
    expired = 3,  // identical native ticket already consumed -> stock
    poisoned = 4, // same issuer ticket published contradictory source -> stock
    invalid = 5   // incomplete native identity; leave original behavior
};

class thread_scope_journal {
    ticket current_{};
    source signal_{};
    bool valid_ = false;
    bool consumed_ = false;
    bool poisoned_ = false;
public:
    void begin_selector() noexcept {
        current_ = {};
        signal_ = {};
        valid_ = consumed_ = poisoned_ = false;
    }

    bool publish(const ticket &t, const source &s) noexcept {
        if (!source_matches_ticket(t, s))
            return false;
        if (valid_ && same_scope(current_, t)) {
            if (!same_source(signal_, s)) {
                poisoned_ = true;
                return false;
            }
            return !poisoned_ && !consumed_;
        }
        // A different selector invalidates the previous one-shot receipt;
        // no global material cache and no stale cross-model source reuse.
        current_ = t;
        signal_ = s;
        valid_ = true;
        consumed_ = poisoned_ = false;
        return true;
    }

    verdict take(const ticket &t, source &out) noexcept {
        out = {};
        if (!well_formed(t))
            return verdict::invalid;
        if (!valid_)
            return verdict::absent;
        if (!same_material(current_.material, t.material) ||
            current_.selector_epoch != t.selector_epoch)
            return verdict::absent;
        if (!same_scope(current_, t))
            return verdict::mismatch;
        if (poisoned_)
            return verdict::poisoned;
        if (consumed_)
            return verdict::expired;
        if (!source_matches_ticket(t, signal_))
            return verdict::poisoned;
        out = signal_;
        consumed_ = true;
        return verdict::exact;
    }
};
inline thread_local thread_scope_journal tls{};
} // namespace dsrrl::runtime::v475
