// No game process or new screen captures are needed. Verify the actual
// producer transport used by experimental metal EnvSpec draw consumers.
#include "dsrrl/runtime/pmetal_producer_state.hpp"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstdint>
#include <thread>

namespace mr = dsrrl::operators::material_response;
namespace rt = dsrrl::runtime;

namespace {
mr::material_identity exact_owner(
    const char *name, std::uint32_t route, std::uint8_t digest_seed)
{
    mr::material_identity x{};
    x.valid = true;
    x.owner_tuple_exact = true;
    x.material_slot_valid = true;
    x.actual_material_exact = true;
    x.material_slot = 2u;
    x.route_index = route;
    x.semantic_name_hash = mr::mtd_semantic_hash(name);
    x.material_family_hash = mr::mtd_semantic_hash("DifSpcBmp");
    x.flver_sha256[0] = digest_seed;
    x.raw_mtd_sha256[0] = digest_seed;
    return x;
}

rt::pmetal_envspec_source source(float value, std::uint32_t row)
{
    rt::pmetal_envspec_source s{};
    s.a = {{value, value * 0.5f, 0.125f}};
    s.b = {{value + 0.125f, 0.25f, 0.5f}};
    s.beta = 0.25f;
    s.bank_signature_a = 0x12340000u + row;
    s.bank_signature_b = 0x12350000u + row;
    s.row_id_a = row;
    s.row_id_b = row + 1u;
    return s;
}

bool right_source(const rt::pmetal_envspec_source &value,
                  const rt::pmetal_envspec_source &expected)
{
    return value.a == expected.a && value.b == expected.b &&
           value.beta == expected.beta &&
           value.bank_signature_a == expected.bank_signature_a &&
           value.row_id_a == expected.row_id_a;
}
} // namespace

int main()
{
    constexpr std::uint64_t epoch = 0x35u;
    const auto alp = exact_owner("P_Metal[DSB]_Alp.mtd", 2u, 0x45u);
    const auto edge = exact_owner("P_Metal[DSB]_Edge.mtd", 5u, 0x8bu);
    const auto cmetal = exact_owner("C_Metal[DSB].mtd", 229u, 0xaeu);
    const auto alp_source = source(1.0f, 10u);
    const auto edge_source = source(2.0f, 20u);
    const auto cmetal_source = source(3.0f, 30u);

    rt::pmetal_envspec_source actual{};
    rt::pmetal_producer_state_clear();
    rt::pmetal_producer_state_begin(alp, epoch);
    assert(!rt::pmetal_producer_state_latest(alp, epoch, actual));
    rt::pmetal_producer_state_publish(alp, alp_source, epoch);
    assert(rt::pmetal_producer_state_latest(alp, epoch, actual));
    assert(right_source(actual, alp_source));
    assert(!rt::pmetal_producer_state_latest(edge, epoch, actual));

    rt::pmetal_producer_state_begin(edge, epoch);
    assert(!rt::pmetal_producer_state_latest(edge, epoch, actual));
    rt::pmetal_producer_state_publish(edge, edge_source, epoch);
    assert(rt::pmetal_producer_state_latest(edge, epoch, actual));
    assert(right_source(actual, edge_source));
    // Even a synchronized-bucket collision may fail open, but may not
    // reinterpret an Edge producer publication as Alp.
    if (rt::pmetal_producer_state_latest(alp, epoch, actual))
        assert(right_source(actual, alp_source));

    rt::pmetal_producer_state_begin(cmetal, epoch);
    rt::pmetal_producer_state_publish(cmetal, cmetal_source, epoch);
    assert(rt::pmetal_producer_state_latest(cmetal, epoch, actual));
    assert(right_source(actual, cmetal_source));

    // Force the actual cross-thread cache path (TLS differs per thread).
    std::thread consumer([&] {
        rt::pmetal_envspec_source value{};
        assert(rt::pmetal_producer_state_latest(cmetal, epoch, value));
        assert(right_source(value, cmetal_source));
        if (rt::pmetal_producer_state_latest(alp, epoch, value))
            assert(right_source(value, alp_source));
        if (rt::pmetal_producer_state_latest(edge, epoch, value))
            assert(right_source(value, edge_source));
    });
    consumer.join();

    // Positive lookup requires the full exact material identity.
    auto wrong = cmetal;
    wrong.flver_sha256[0] ^= 1u;
    assert(!rt::pmetal_producer_state_latest(wrong, epoch, actual));
    wrong = cmetal;
    wrong.material_slot ^= 1u;
    assert(!rt::pmetal_producer_state_latest(wrong, epoch, actual));
    wrong = cmetal;
    wrong.raw_mtd_sha256[0] ^= 1u;
    assert(!rt::pmetal_producer_state_latest(wrong, epoch, actual));
    wrong = cmetal;
    wrong.semantic_name_hash ^= 1u;
    assert(!rt::pmetal_producer_state_latest(wrong, epoch, actual));
    wrong = cmetal;
    wrong.route_index = 345u;
    assert(!rt::pmetal_producer_state_latest(wrong, epoch, actual));

    assert(!rt::pmetal_producer_state_latest(cmetal, epoch + 1u, actual));
    // Selector begin invalidates only its exact producer bucket until publish.
    rt::pmetal_producer_state_begin(cmetal, epoch);
    assert(!rt::pmetal_producer_state_latest(cmetal, epoch, actual));
    rt::pmetal_producer_state_publish(cmetal, cmetal_source, epoch);
    rt::pmetal_producer_state_clear();
    assert(rt::pmetal_producer_state_latest(cmetal, epoch, actual));
    assert(right_source(actual, cmetal_source));
    return 0;
}
