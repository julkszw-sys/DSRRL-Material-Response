#include "dsrrl/runtime/pmetal_selector_policy.hpp"
#include <array>
#include <cstdlib>
#include <limits>
#include <vector>
#include <utility>

using namespace dsrrl::runtime::pmetal_selector_policy;
static void check(bool ok) { if (!ok) std::abort(); }
int main()
{
    check(is_synthetic_zero_descriptor(0x20EB7Fu,0u,0u,0.0f));
    check(!is_synthetic_zero_descriptor(0x20E019u,0u,0u,0.0f));
    check(!is_synthetic_zero_descriptor(0x20EB7Fu,0x0820u,0u,0.0f));
    check(!is_synthetic_zero_descriptor(0x20EB7Fu,0u,0u,0.5f));
    int a = 1, common = 2;
    std::array<std::array<int *,7>,12> banks{};
    std::vector<std::pair<unsigned,unsigned>> calls;
    auto lookup = [&](unsigned area, unsigned type) -> int * {
        check(area < banks.size() && (type == 5 || type == 6));
        calls.emplace_back(area,type); return banks[area][type];
    };
    banks[0][5] = &a; banks[3][6] = &a; banks[11][5] = &common;
    check(source(0,false,lookup) == &a);
    calls.clear();
    check(source(0x0312,true,lookup) == &a);
    check(calls.size() == 1 && calls[0] == std::make_pair(3u,6u));
    calls.clear();
    check(source(0x0412,true,lookup) == &common);
    check(calls == std::vector<std::pair<unsigned,unsigned>>{{4,6},{11,6},{11,5}});
    calls.clear();
    check(source(0x7f12,false,lookup) == &common);
    check(calls == std::vector<std::pair<unsigned,unsigned>>{{11,5}});
    banks[11][6] = &a;
    check(source(0x0412,true,lookup) == &a);
    banks[11][5] = nullptr;
    check(source(0x0412,false,lookup) == nullptr);
    auto e = select(0x0312,0x0423,0.25f);
    check(e.valid && e.a == 0x0312 && e.b == 0x0423 && e.beta == 0.25f);
    e = select(0x0312,-1,0.0f); check(e.valid && e.a == e.b && e.beta == 0);
    e = select(-1,0x0423,1.0f); check(e.valid && e.a == 0x0423 && e.a == e.b);
    e = select(0x0312,0x0312,0.8f); check(e.valid && e.beta == 0);
    check(!select(-1,0x0423,0.25f).valid);
    check(!select(1,2,std::numeric_limits<float>::quiet_NaN()).valid);
    check(!select(1,2,std::numeric_limits<float>::infinity()).valid);
}
