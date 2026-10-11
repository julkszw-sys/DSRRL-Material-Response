#include "dsrrl/runtime/v214_resource_name_provenance.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cwchar>
#include <iostream>

using dsrrl::runtime::v214_resource_provenance::registry;
using dsrrl::runtime::v214_resource_provenance::status;

static void check(bool value) {
    if (!value) std::abort();
}

static status get(registry &r, std::uintptr_t device,
                  std::uintptr_t resource, const wchar_t *expected) {
    std::array<wchar_t,257> name{};
    std::size_t count=0u;
    const auto state=r.snapshot(device,resource,name,count);
    if (expected!=nullptr) {
        check(state==status::exact);
        check(count==std::wcslen(expected));
        check(std::wcscmp(name.data(),expected)==0);
    } else {
        check(state!=status::exact);
    }
    return state;
}

int main() {
    registry r;
    constexpr std::uintptr_t d1=0x1001u,d2=0x1002u;
    constexpr std::uintptr_t s1=0x2001u,s2=0x2002u;
    check(get(r,d1,s1,nullptr)==status::absent);
    check(r.observe(d1,s1,L"HD_A_9550_s",11u));
    check(get(r,d1,s1,L"HD_A_9550_s")==status::exact);
    check(r.observe(d1,s1,L"HD_A_9550_s",11u));
    check(get(r,d1,s2,nullptr)==status::absent);
    check(get(r,d2,s1,nullptr)==status::ambiguous);
    // Reject cross-resource hijack and incomplete/unsafe source names.
    check(!r.observe(0u,s1,L"HD_A_9550_s",11u));
    check(!r.observe(d1,s2,L"bad name_s",10u));
    check(!r.observe(d1,s2,L"bad\nname_s",10u));
    check(r.observe(d1,s2,L"HD_A_9570_s",11u));
    // Different names at the same native resource permanently poison it.
    check(!r.observe(d1,s1,L"HD_A_9570_s",11u));
    check(get(r,d1,s1,nullptr)==status::ambiguous);
    check(!r.observe(d1,s1,L"HD_A_9550_s",11u));
    // Native resource destruction retires aliases and their quarantines.
    r.destroy(d1,s1);
    check(get(r,d1,s1,nullptr)==status::absent);
    check(r.observe(d1,s1,L"HD_A_9570_s",11u));
    check(get(r,d1,s1,L"HD_A_9570_s")==status::exact);
    r.destroy_device(d1);
    check(get(r,d1,s1,nullptr)==status::absent);
    check(get(r,d1,s2,nullptr)==status::absent);
    std::cout<<"V214_EXACT_RESOURCE_NAME_REGRESSION_PASS source_exact=1 lifetime=1 conflict_failopen=1 stale_reject=1\n";
}
