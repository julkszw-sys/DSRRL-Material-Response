// Strict offline A/B oracle. No runtime hooks, game process, GPU or sidecars.
// Builds as standalone C++17 and compares the original literal table against
// immutable, constexpr-predecoded records on all 36 canonical tuples and
// targeted fail-open mutations. Benchmarks are *host-runner* CPU, not game FPS.
#include "dsrrl/runtime/v203_equipment_constexpr_lookup.hpp"
#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

// Link-time stubs reproduce the exact source-code FNV algorithm and a pure
// authority gate so BOTH compared implementations use the same precondition.
// NOT a replacement implementation of the production owner tuple authenticator.
namespace dsrrl::operators::material_response {
std::uint64_t mtd_semantic_hash(const char *text) noexcept {
    if (!text) return 0u;
    std::uint64_t h = 14695981039346656037ull;
    for (; *text; ++text) {
        h ^= static_cast<std::uint8_t>(*text);
        h *= 1099511628211ull;
    }
    return h;
}
bool has_exact_flver_material_ownership(
    const mtd_semantic_query &query) noexcept {
    return query.material.valid &&
           query.material.semantic_name_hash != 0u &&
           query.ownership.exact &&
           query.ownership.material_slot_valid &&
           query.ownership.material_slot == query.material.material_slot &&
           query.ownership.flver_sha256 == query.material.flver_sha256;
}
}

using namespace dsrrl::runtime::v203_equipment;
using namespace dsrrl::operators::material_response;
namespace fast = dsrrl::runtime::v203_equipment::constexpr_lookup;

struct case_record {
    mtd_semantic_query q{};
    std::uint64_t spec_hash=0u;
};

case_record make(
    const flver_slot_spec_row &old) {
    case_record c{};
    auto &m=c.q.material;
    m.valid=true;
    m.owner_tuple_exact=true;
    m.actual_material_exact=true;
    m.material_slot_valid=true;
    m.material_slot=old.slot;
    m.semantic_name_hash=mtd_semantic_hash(old.mtd);
    m.flver_sha256=fast::decode_sha256(old.flver_sha256);
    c.q.ownership.flver_sha256=m.flver_sha256;
    c.q.ownership.material_slot=m.material_slot;
    c.q.ownership.material_slot_valid=true;
    c.q.ownership.exact=true;
    c.spec_hash=logical_spec_hash(old.spec);
    return c;
}

std::uint64_t checks=0;
void assert_equivalent(const case_record &c,
                       bool expected_owner,
                       bool expected_spec) {
    const bool old_owner=exact_proven_owner_slot(c.q);
    const bool new_owner=fast::exact_proven_owner_slot_fast(c.q);
    const bool old_spec=exact_proven_slot_spec_hash(c.q,c.spec_hash);
    const bool new_spec=fast::exact_proven_slot_spec_hash_fast(c.q,c.spec_hash);
    if(old_owner!=new_owner || old_spec!=new_spec ||
       old_owner!=expected_owner || old_spec!=expected_spec) {
        std::fprintf(stderr,
            "MISMATCH original-owner=%d fast-owner=%d original-spec=%d fast-spec=%d expected=%d/%d\n",
            int(old_owner),int(new_owner),int(old_spec),int(new_spec),
            int(expected_owner),int(expected_spec));
        std::exit(3);
    }
    ++checks;
}

template<class F>
double bench_ns_per_lookup(const std::vector<case_record> &queries,
                           F f, std::uint32_t loops) {
    volatile std::uint64_t sink=0;
    const auto start=std::chrono::steady_clock::now();
    for(std::uint32_t loop=0;loop<loops;++loop)
        for(const auto &c:queries)
            sink+=f(c) ? 1u : 0u;
    const auto end=std::chrono::steady_clock::now();
    if(sink==0xdeadbeefull)std::fprintf(stderr,"sink\n");
    const auto elapsed=std::chrono::duration<double,std::nano>(end-start).count();
    return elapsed/(double(loops)*double(queries.size()));
}

int main() {
    std::vector<case_record> valid, invalid;
    for(std::size_t i=0;i<36u;++i) {
        const auto &r=k_proven_parts_specs[i];
        if(!fast::valid_digest_literal(r.flver_sha256) ||
           fast::fnv1a(r.mtd)!=mtd_semantic_hash(r.mtd) ||
           fast::fnv1a(r.spec,true)!=logical_spec_hash(r.spec))
            return 2;
        auto v=make(r);
        assert_equivalent(v,true,true);
        valid.push_back(v);
        auto neg=v;neg.q.material.flver_sha256[0]^=1u;
        neg.q.ownership.flver_sha256=neg.q.material.flver_sha256;
        assert_equivalent(neg,false,false);invalid.push_back(neg);
        neg=v;neg.q.material.material_slot=999u;
        neg.q.ownership.material_slot=999u;
        assert_equivalent(neg,false,false);invalid.push_back(neg);
        neg=v;neg.q.material.semantic_name_hash^=1u;
        assert_equivalent(neg,false,false);invalid.push_back(neg);
        neg=v;neg.spec_hash^=1u;
        assert_equivalent(neg,true,false);invalid.push_back(neg);
        neg=v;neg.q.material.owner_tuple_exact=false;
        assert_equivalent(neg,false,false);invalid.push_back(neg);
        neg=v;neg.q.material.actual_material_exact=false;
        assert_equivalent(neg,false,false);invalid.push_back(neg);
        neg=v;neg.q.ownership.exact=false;
        assert_equivalent(neg,false,false);invalid.push_back(neg);
        neg=v;neg.q.ownership.material_slot_valid=false;
        assert_equivalent(neg,false,false);invalid.push_back(neg);
        neg=v;neg.q.ownership.flver_sha256[0]^=1u;
        assert_equivalent(neg,false,false);invalid.push_back(neg);
        neg=v;neg.q.material.flver_identity_hash=0xffffffffffffffffull;
        assert_equivalent(neg,true,true); // auxiliary token is NOT an authority
        neg=v;neg.spec_hash=0u;
        assert_equivalent(neg,true,false);invalid.push_back(neg);
    }
    // Shared FLVERs and MTDs: every other MATERIAL SLOT / logical Spec name
    // is checked against the entire original corpus, not just expected false.
    for(std::size_t i=0;i<valid.size();++i)
        for(std::size_t j=0;j<valid.size();++j) {
            auto probe=valid[i];
            probe.spec_hash=valid[j].spec_hash;
            const bool owner=exact_proven_owner_slot(probe.q);
            const bool spec=exact_proven_slot_spec_hash(probe.q,probe.spec_hash);
            assert_equivalent(probe,owner,spec);
        }
    // Deterministic adversarial fuzz with valid authority flags but distinct
    // whole FLVER SHA, MTD and stock spec identities.
    std::uint64_t rng=0x9e3779b97f4a7c15ull;
    for(int k=0;k<4096;++k) {
        rng^=rng<<13; rng^=rng>>7; rng^=rng<<17;
        auto c=valid[static_cast<std::size_t>(rng%valid.size())];
        c.q.material.flver_sha256[static_cast<std::size_t>((rng>>12)%32u)]^=
            static_cast<std::uint8_t>(1u+((rng>>19)&127u));
        c.q.ownership.flver_sha256=c.q.material.flver_sha256;
        assert_equivalent(c,false,false);
    }
    std::fprintf(stdout,"EXACT_OWNER_CONSTEXPR_EQUIVALENCE_PASS checks=%llu rows=36 negatives=%zu\n",
      static_cast<unsigned long long>(checks),invalid.size());
    const unsigned loops=4000u;
    const auto slow_owner=bench_ns_per_lookup(valid,
        [](const case_record &c){return exact_proven_owner_slot(c.q);},loops);
    const auto fast_owner=bench_ns_per_lookup(valid,
        [](const case_record &c){return fast::exact_proven_owner_slot_fast(c.q);},loops);
    const auto slow_spec=bench_ns_per_lookup(valid,
        [](const case_record &c){return exact_proven_slot_spec_hash(c.q,c.spec_hash);},loops);
    const auto fast_spec=bench_ns_per_lookup(valid,
        [](const case_record &c){return fast::exact_proven_slot_spec_hash_fast(c.q,c.spec_hash);},loops);
    const auto slow_miss=bench_ns_per_lookup(invalid,
        [](const case_record &c){return exact_proven_slot_spec_hash(c.q,c.spec_hash);},loops);
    const auto fast_miss=bench_ns_per_lookup(invalid,
        [](const case_record &c){return fast::exact_proven_slot_spec_hash_fast(c.q,c.spec_hash);},loops);
    std::fprintf(stdout,
        "HOST_ONLY_BENCHMARK_NOT_GAME_FPS owner_old_ns=%.3f owner_fast_ns=%.3f spec_old_ns=%.3f spec_fast_ns=%.3f miss_old_ns=%.3f miss_fast_ns=%.3f\n",
        slow_owner,fast_owner,slow_spec,fast_spec,slow_miss,fast_miss);
    return 0;
}
