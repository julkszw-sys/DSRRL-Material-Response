// PR292: exercise the real native writer registry reset on a 1 MiB
// Windows thread stack, matching the common default stack reservation.
#include "dsrrl/runtime/pmetal_native_cb_writer_trace.hpp"
#include <array>
#include <cstdint>
#include <cstdio>

int main() {
    using namespace dsrrl::runtime;
    constexpr std::uintptr_t key=0x123450u;
    pmetal_native_cb_writer_reset();
    pmetal_native_cb_writer_watch(key,2064u);
    std::array<unsigned char,2064> data{};
    for (std::size_t i=0u;i<data.size();++i)
        data[i]=static_cast<unsigned char>(i&255u);
    pmetal_native_cb_writer_before_update(
        key,data.data(),0u,data.size());
    const auto observed=pmetal_native_cb_writer_lookup(key);
    if (!observed.complete || observed.register_count!=129u ||
        observed.epoch==0u || observed.byte_count!=2064u ||
        observed.hash==0u) {
        std::fputs("PR292 writer watch/epoch/register stamp FAIL\n",stderr);
        return 1;
    }
    pmetal_native_cb_writer_reset();
    const auto cleared=pmetal_native_cb_writer_lookup(key);
    if (cleared.watched || cleared.epoch!=0u) {
        std::fputs("PR292 registry reset FAIL\n",stderr);
        return 2;
    }
    std::puts("PR292_WIN_1MIB_STACK_RESET_SMOKE_PASS");
    return 0;
}
