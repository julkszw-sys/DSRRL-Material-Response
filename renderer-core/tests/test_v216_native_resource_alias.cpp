#include "dsrrl/runtime/v216_native_resource_alias.hpp"
#include <cstdlib>
#include <iostream>
using dsrrl::runtime::v216_native_resource_alias::anchor;
using dsrrl::runtime::v216_native_resource_alias::registry;
#define CHECK(p) do {if(!(p))std::abort();}while(0)
int main() {
    registry r;
    constexpr std::uintptr_t d1=0x1100,d2=0x1200,rs=0x2200,v1=0x3300,v2=0x3400;
    constexpr std::uint64_t h1=0xabcdef1234567890ull,h2=0x1177339900aabbccull;
    anchor out{};
    CHECK(!r.resolve(d1,rs,out));
    CHECK(!r.observe(d1,rs,v1,0));
    CHECK(r.observe(d1,rs,v1,h1));
    CHECK(r.resolve(d1,rs,out) && out.view==v1 && out.logical_hash==h1);
    CHECK(!r.resolve(d2,rs,out)); // device cannot borrow a resource
    CHECK(r.observe(d1,rs,v2,h1)); // true alias, anchor unchanged
    CHECK(r.resolve(d1,rs,out) && out.view==v1);
    CHECK(!r.observe(d1,rs,v2,h2)); // conflicting same resource/name
    CHECK(!r.resolve(d1,rs,out));
    r.destroy_view(v2); // conflict remains quarantined
    CHECK(!r.resolve(d1,rs,out));
    r.destroy_resource(d1,rs);
    CHECK(!r.resolve(d1,rs,out));
    CHECK(r.observe(d1,rs,v2,h2)); // reuse only after resource destroyed
    CHECK(r.resolve(d1,rs,out) && out.view==v2 && out.logical_hash==h2);
    r.destroy_view(v2);
    CHECK(!r.resolve(d1,rs,out)); // anchor retirement
    CHECK(r.observe(d1,rs,v1,h1));
    r.destroy_device(d1);
    CHECK(!r.resolve(d1,rs,out));
    CHECK(r.observe(d1,rs,v1,h1));
    CHECK(!r.observe(d1,0x2244,v1,h1)); // view cannot own two resources
    CHECK(!r.resolve(d1,rs,out)); // quarantined old
    r.destroy_resource(d1,rs);
    CHECK(!r.resolve(d1,rs,out));
    std::cout << "V216_NATIVE_RESOURCE_ALIAS_PASS exact_resource=1 alias=1 conflict=1 lifetime=1 device=1\n";
}
