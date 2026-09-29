#include "dsrrl/operators/material_response/material_response_island.hpp"
#include "dsrrl/operators/material_response/generated_dsr_flver_owner_tuples_v1.hpp"
#include "dsrrl/operators/material_response/generated_pointlight_material_authority_v1.hpp"

#include <array>
#include <cstdint>
#include <cstring>
#include <iostream>

#define CHECK(x) do { if (!(x)) { std::cerr << "CHECK failed: " #x "\n"; return 1; } } while(false)

namespace {

float f32(std::uint32_t bits)
{
    float value = 0.0f;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

bool make_identity(
    const dsrrl::operators::material_response::generated::
        pointlight_material_authority_record_v1 &record,
    dsrrl::operators::material_response::material_identity &out)
{
    namespace gen =
        dsrrl::operators::material_response::generated;

    for (const auto &group : gen::k_dsr_flver_owner_groups) {
        for (std::uint32_t slot = 0u;
             slot < group.material_count;
             ++slot) {
            const auto index =
                static_cast<std::size_t>(
                    group.first_material + slot);
            if (index >=
                    gen::k_dsr_flver_owner_mtd_hashes.size())
                return false;
            if (gen::k_dsr_flver_owner_mtd_hashes[index] !=
                    record.semantic_name_hash)
                continue;

            out = {};
            out.valid = true;
            out.flver_sha256 = group.flver_sha256;
            out.material_slot = slot;
            out.material_slot_valid = true;
            out.owner_tuple_exact = true;
            out.semantic_name_hash =
                record.semantic_name_hash;
            out.raw_mtd_sha256 =
                record.raw_mtd_sha256;
            return true;
        }
    }

    return false;
}

dsrrl::operators::material_response::material_identity
make_runtime_identity(
    const dsrrl::operators::material_response::generated::
        pointlight_material_authority_record_v1 &record)
{
    dsrrl::operators::material_response::material_identity out{};
    out.valid = true;
    out.actual_material_exact = true;
    out.semantic_name_hash =
        record.semantic_name_hash;
    out.raw_mtd_sha256 =
        record.raw_mtd_sha256;
    out.route_index =
        0xA0000000u |
        (record.router_index & 0x0000ffffu);
    return out;
}

} // namespace

int main()
{
    namespace mr =
        dsrrl::operators::material_response;
    namespace gen =
        dsrrl::operators::material_response::generated;

    static_assert(
        gen::k_pointlight_material_authority_v1.size() ==
        205u);

    std::size_t spc_count = 0u;
    std::size_t nospc_count = 0u;
    std::size_t rgb_c101_count = 0u;

    for (const auto &record :
         gen::k_pointlight_material_authority_v1) {
        if (record.spc) {
            ++spc_count;
            if (!record.c101_scalar)
                ++rgb_c101_count;
        } else {
            ++nospc_count;
        }
    }

    CHECK(spc_count == 180u);
    CHECK(nospc_count == 25u);
    CHECK(rgb_c101_count == 12u);

    const auto *scalar_spc =
        gen::find_pointlight_material_authority_v1(
            0x5bcac2f17b140f9cull);
    const auto *rgb_spc =
        gen::find_pointlight_material_authority_v1(
            0xfde69928da2a1e02ull);
    const auto *nospc =
        gen::find_pointlight_material_authority_v1(
            0x845feea1ec353cbaull);

    CHECK(scalar_spc != nullptr);
    CHECK(scalar_spc->spc);
    CHECK(scalar_spc->c101_scalar);
    CHECK(rgb_spc != nullptr);
    CHECK(rgb_spc->spc);
    CHECK(!rgb_spc->c101_scalar);
    CHECK(nospc != nullptr);
    CHECK(!nospc->spc);

    mr::material_response_island island;
    CHECK(island.finalize_registration());

    mr::material_identity identity{};

    identity = make_runtime_identity(*nospc);
    auto decision =
        island.evaluate_direct_pointlight_material(
            identity,
            false);
    CHECK(decision.active);
    CHECK(decision.reason == mr::decision_reason::active);
    CHECK((decision.certified_operations &
           mr::diffuse_material_domain_linear) != 0u);
    CHECK((decision.certified_operations &
           mr::specular_factor_c101) == 0u);
    CHECK(decision.c100[0] ==
          f32(nospc->c100_bits[0]));
    // The established 25 NoSpc authority keeps its historical telemetry
    // route namespace; the new corpus extends materials without rewriting it.
    CHECK((decision.route_index & 0x80000000u) != 0u);

    identity = make_runtime_identity(*scalar_spc);
    decision =
        island.evaluate_direct_pointlight_material(
            identity,
            true);
    CHECK(decision.active);
    CHECK((decision.certified_operations &
           mr::specular_factor_c101) != 0u);
    CHECK(decision.c101 ==
          f32(scalar_spc->c101_bits[0]));
    CHECK((decision.route_index & 0xF0000000u) ==
          0xA0000000u);
    CHECK(decision.ptde_specular_power_verified);
    CHECK(decision.ptde_specular_power ==
          f32(scalar_spc->c102_bits));

    const auto wrong_family =
        island.evaluate_direct_pointlight_material(
            identity,
            false);
    CHECK(!wrong_family.active);
    CHECK(wrong_family.reason ==
          mr::decision_reason::no_certified_operator);

    identity = make_runtime_identity(*rgb_spc);
    const auto rgb_deferred =
        island.evaluate_direct_pointlight_material(
            identity,
            true);
    CHECK(!rgb_deferred.active);
    CHECK(rgb_deferred.reason ==
          mr::decision_reason::no_certified_operator);

    identity.raw_mtd_sha256 = {};
    const auto wrong_raw =
        island.evaluate_direct_pointlight_material(
            identity,
            true);
    CHECK(!wrong_raw.active);
    CHECK(wrong_raw.reason ==
          mr::decision_reason::owner_tuple_not_authenticated);

    const auto *raw_scalar =
        gen::find_pointlight_material_authority_by_raw_mtd_v1(
            scalar_spc->raw_mtd_sha256);
    CHECK(raw_scalar != nullptr);
    CHECK(gen::pointlight_material_behavior_equal_v1(
        *raw_scalar,
        *scalar_spc));

    const auto *conflict_record =
        gen::find_pointlight_material_authority_v1(
            0x5e35fabde6b7756eull);
    CHECK(conflict_record != nullptr);
    CHECK(gen::find_pointlight_material_authority_by_raw_mtd_v1(
        conflict_record->raw_mtd_sha256) == nullptr);

    std::cout <<
        "pointlight_material_authority_tests: PASS "
        "205 exact rows; 180 Spc / 25 NoSpc; "
        "12 RGB-c101 rows remain fail-open\n";
    return 0;
}
