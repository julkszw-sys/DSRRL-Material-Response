// Independent C++17 oracle for EXACT leather raw-MTD identity.
// No DSR executable, GPU, game timing or renderer mutation.
#include "dsrrl/runtime/v203_equipment_exact_mtd_router.hpp"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <vector>

namespace mr = dsrrl::operators::material_response;
namespace gen = mr::generated;
namespace fast = dsrrl::runtime::v203_equipment::mtd_fast;
using query = mr::mtd_semantic_query;

mr::mtd_envspec_semantics original(const query &q) noexcept {
    mr::mtd_envspec_semantics out{};
    if (!q.material.valid || !q.material.semantic_name_hash) return out;
    for (const auto &r : gen::k_envspec_router_v1) {
        if (q.material.semantic_name_hash != r.semantic_name_hash ||
            q.material.raw_mtd_sha256 != r.raw_mtd_sha256) continue;
        out.exact_identity_match = true;
        out.envspc_slot_valid = r.envspc_slot < 4u;
        out.envspc_slot = r.envspc_slot;
        out.suppress_dsr_only_safe = r.explicit_none_safe;
        switch (r.state) {
        case gen::envspec_router_state::present:
            out.presence = mr::ptde_envspec_presence::present;
            out.router_state = mr::mtd_envspec_router_state::present; break;
        case gen::envspec_router_state::explicit_none:
            out.presence = mr::ptde_envspec_presence::absent;
            out.router_state = mr::mtd_envspec_router_state::explicit_none; break;
        case gen::envspec_router_state::nospc_host:
            out.presence = mr::ptde_envspec_presence::absent;
            out.router_state = mr::mtd_envspec_router_state::nospc_host;
            out.suppress_dsr_only_safe = false; break;
        default:
            out.suppress_dsr_only_safe = false;
        }
        return out;
    }
    return out;
}
bool same(const mr::mtd_envspec_semantics &a,
          const mr::mtd_envspec_semantics &b) noexcept {
    return a.presence == b.presence &&
        a.router_state == b.router_state &&
        a.exact_identity_match == b.exact_identity_match &&
        a.suppress_dsr_only_safe == b.suppress_dsr_only_safe &&
        a.envspc_slot_valid == b.envspc_slot_valid &&
        a.envspc_slot == b.envspc_slot;
}
query make(std::size_t index) {
    const auto &r = gen::k_envspec_router_v1[index];
    query q{};
    q.material.valid = true;
    q.material.semantic_name_hash = r.semantic_name_hash;
    q.material.raw_mtd_sha256 = r.raw_mtd_sha256;
    return q;
}
std::uint64_t checks = 0;
void verify(const query &q) {
    const auto a = original(q), b = fast::classify(q);
    if (!same(a,b)) {
        std::fprintf(stderr,"raw MTD classifier mismatch test=%llu hash=%llx original=%u/%u new=%u/%u\n",
            static_cast<unsigned long long>(checks),
            static_cast<unsigned long long>(q.material.semantic_name_hash),
            unsigned(a.exact_identity_match),unsigned(a.envspc_slot),
            unsigned(b.exact_identity_match),unsigned(b.envspc_slot));
        std::exit(7);
    }
    ++checks;
}
template<class F> double bench(
    const std::vector<query> &inputs, F fn) {
    volatile std::uint64_t sink = 0;
    const auto start = std::chrono::steady_clock::now();
    for (std::uint32_t iteration=0; iteration<2500; ++iteration)
        for (const auto &q : inputs)
            sink += fn(q).exact_identity_match ? 1u : 0u;
    const auto end = std::chrono::steady_clock::now();
    if (sink == 0x123456789abcdef0ull) std::abort();
    return std::chrono::duration<double,std::nano>(end-start).count()
         / (2500.0 * inputs.size());
}
int main() {
    std::vector<query> positives, negatives;
    for (const auto i : fast::k_indices) {
        query q = make(i);
        verify(q);
        if (!original(q).exact_identity_match) return 2;
        positives.push_back(q);
        // One-bit and byte perturbations in ALL 32 raw-MTD SHA bytes.
        for (std::size_t n=0;n<32;++n) {
            for (std::uint8_t bit=0;bit<8;++bit) {
                query mutated=q;
                mutated.material.raw_mtd_sha256[n] ^= (1u<<bit);
                verify(mutated);
                negatives.push_back(mutated);
            }
        }
        q.material.valid = false; verify(q);
        q = make(i);
        q.material.semantic_name_hash = 0u; verify(q);
        q = make(i);
        q.material.semantic_name_hash ^= 1u;
        verify(q);
    }
    // Wrong variant SHA under right MTD hash must not inherit EnvSpec.
    for (const auto a : fast::k_indices)
        for (const auto b : fast::k_indices) {
            query q=make(a);
            q.material.raw_mtd_sha256 =
                gen::k_envspec_router_v1[b].raw_mtd_sha256;
            verify(q);
        }
    // Foreign generated MTD records are never routed via this function:
    // verify they cannot masquerade as either known leather name.
    for (std::size_t i=0;i<gen::k_envspec_router_record_count;++i) {
        query q=make(i);
        if (q.material.semantic_name_hash == fast::k_dull ||
            q.material.semantic_name_hash == fast::k_leather) {
            verify(q);
            continue;
        }
        if (fast::classify(q).exact_identity_match)
            return 3;
        q.material.semantic_name_hash = fast::k_leather;
        verify(q);
        q.material.semantic_name_hash = fast::k_dull;
        verify(q);
    }
    // The last two rewritten name-hash cases are in the selected domain,
    // so both implementations must return the identical UNKNOWN signal.
    std::vector<query> sampled(positives);
    sampled.insert(sampled.end(),negatives.begin(),negatives.begin()+32);
    const auto before=bench(sampled,original);
    const auto after=bench(sampled,fast::classify);
    std::printf("V203_RAW_MTD_ORACLE_PASS checks=%llu exact_variants=3 dull=2 leather=1\n",
        static_cast<unsigned long long>(checks));
    std::printf("HOST_ONLY ns_per_lookup old_325=%.3f new_3=%.3f; not game FPS\n",
        before,after);
}
