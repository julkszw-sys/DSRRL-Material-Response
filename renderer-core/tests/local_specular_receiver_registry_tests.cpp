#include "dsrrl/operators/point_light/local_specular_receiver_registry.hpp"

#include <array>
#include <cstdint>
#include <iostream>

using namespace dsrrl::operators::point_light;

namespace {

bool check(bool condition,const char *expr,int line)
{
    if(condition) return true;
    std::cerr<<"CHECK FAILED line "<<line<<": "<<expr<<'\n';
    return false;
}
#define CHECK(e) do { if(!check(static_cast<bool>(e),#e,__LINE__)) return 1; } while(false)

std::array<std::uint8_t,32> digest(const char *hex)
{
    std::array<std::uint8_t,32> out{};
    auto nibble=[](char c)->std::uint8_t {
        if(c>='0'&&c<='9') return static_cast<std::uint8_t>(c-'0');
        if(c>='a'&&c<='f') return static_cast<std::uint8_t>(10+c-'a');
        return 0xffu;
    };
    for(std::size_t i=0;i<out.size();++i)
        out[i]=static_cast<std::uint8_t>((nibble(hex[i*2])<<4)|nibble(hex[i*2+1]));
    return out;
}

} // namespace

int main()
{
    CHECK(local_specular_receiver_count()==72u);

    local_specular_receiver_identity id{};

    CHECK(local_specular_receiver_for_digest(
        digest("fc93eb2c09aab6cdd3000e4fedc82bacd522094e45aac3e3385525391656b368"),
        21760u,id));
    CHECK(id.receiver_class==local_specular_receiver_class::clustered_spc_pnts);
    CHECK(id.representative_shader_index==728u);
    CHECK(id.alias_count==2u);

    CHECK(local_specular_receiver_for_digest(
        digest("021cbb62d1cca4bad3789c50c3aa551b893904910dbf6596aedb369814d9001f"),
        24564u,id));
    CHECK(id.receiver_class==local_specular_receiver_class::fixed_spc_pntss);
    CHECK(id.representative_shader_index==729u);

    CHECK(local_specular_receiver_for_digest(
        digest("6869c41a14152fc12a1f798148b9e10343387e874cb0454078cc36595260b945"),
        28180u,id));
    CHECK(id.receiver_class==local_specular_receiver_class::fixed_spc_pntssss);
    CHECK(id.representative_shader_index==730u);

    auto bad=digest("fc93eb2c09aab6cdd3000e4fedc82bacd522094e45aac3e3385525391656b368");
    bad[0]^=0xffu;
    CHECK(!local_specular_receiver_for_digest(bad,21760u,id));
    CHECK(id.receiver_class==local_specular_receiver_class::unsupported);

    CHECK(!local_specular_receiver_for_digest(
        digest("fc93eb2c09aab6cdd3000e4fedc82bacd522094e45aac3e3385525391656b368"),
        21764u,id));

    std::cout<<"local_specular_receiver_registry_tests: PASS\n";
    return 0;
}
