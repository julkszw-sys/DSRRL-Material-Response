#include "dsrrl/runtime/native_ps_t1_view_lifetime.hpp"
#include <cstdio>
#define CHECK(expr) do { if (!(expr)) { std::fprintf(stderr, "SPC25 epoch check failed line %d: %s\n", __LINE__, #expr); return 1; } } while (false)
int main() {
    dsrrl::runtime::native_ps_t1_lifetime::registry<2> r;
    CHECK(!r.current(10,20));
    const auto a=r.init(1,10,20);
    CHECK(a!=0 && r.current(10,20).epoch==a);
    CHECK(!r.current(10,21));
    CHECK(r.init(1,10,20)==0 && !r.current(10,20));
    r.destroy(1,10);
    const auto b=r.init(1,10,20);
    CHECK(b!=0 && b!=a);
    r.destroy(1,10);
    const auto c=r.init(2,10,21);
    CHECK(c!=0 && c!=b && !r.current(10,20));
    CHECK(r.current(10,21).device==2);
    r.destroy_device(2);
    CHECK(!r.current(10,21));
    CHECK(r.init(3,10,20)!=0);
    CHECK(r.init(4,11,21)!=0);
    CHECK(r.init(5,12,22)==0);
    r.destroy(3,10);
    CHECK(r.init(5,12,22)!=0);
    CHECK(r.init(7,12,22)==0 && !r.current(12,22));
    r.destroy_device(5);
    CHECK(!r.current(12,22));
    const auto d=r.init(6,12,22);
    CHECK(d!=0 && d>c);
    CHECK(r.current(12,22).epoch==d);
    // View address alias while active on a different device must poison.
    CHECK(r.init(7,12,22)==0);
    CHECK(!r.current(12,22));
    r.destroy_device(6);
    CHECK(r.init(8,12,22)!=0);
    CHECK(r.current(12,22).device==8);

    // Saturate a single 16-probe hash bucket. Never fall through to a
    // different identity when capacity is exhausted.
    dsrrl::runtime::native_ps_t1_lifetime::registry<32> crowded;
    for (std::uintptr_t i = 0; i < 16u; ++i)
        CHECK(crowded.init(1, 0x10000u + i, 0x30000u + i) != 0u);
    CHECK(crowded.init(1, 0x20000u, 0x40000u) == 0u);
    for (std::uintptr_t i = 0; i < 16u; ++i)
        CHECK(crowded.current(0x10000u + i, 0x30000u + i));
    crowded.destroy(1, 0x10000u);
    // Duplicate live key exists behind the newly free tombstone.
    // It must be poisoned, never treated as a new valid epoch.
    CHECK(crowded.init(1, 0x10003u, 0x30003u) == 0u);
    CHECK(!crowded.current(0x10003u, 0x30003u));
    CHECK(crowded.init(1, 0x20000u, 0x40000u) != 0u);
    CHECK(crowded.current(0x20000u, 0x40000u));
}
