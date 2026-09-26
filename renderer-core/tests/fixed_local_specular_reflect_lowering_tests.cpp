#include "dsrrl/operators/point_light/fixed_local_specular_reflect_lowering.hpp"

#include <cstdint>
#include <iostream>

using namespace dsrrl::operators::point_light;

namespace {
bool check(bool c,const char *e,int l)
{
    if(c) return true;
    std::cerr<<"CHECK FAILED line "<<l<<": "<<e<<'\n';
    return false;
}
#define CHECK(e) do { if(!check(static_cast<bool>(e),#e,__LINE__)) return 1; } while(false)
}

int main()
{
    // Real token shape from
    // FRPG_Phn_DifSpcBmpMulLitCsd_HemEnvPntSS.fpo.
    fixed_local_specular_light_operands in{};
    in.normal.token = {{0x00100246u,5u}};
    in.view.token = {{0x00100246u,6u}};
    in.light.token = {{0x00100246u,10u}};
    in.primary_scalar_dst.token = {{0x00100042u,3u}}; // r3.z
    in.auxiliary_scalar_dst.token = {{0x00100082u,3u}}; // r3.w
    in.ndotl_scalar_dst.token = {{0x00100082u,4u}}; // r4.w

    fixed_local_specular_reflect_lowering out{};
    CHECK(emit_fixed_local_specular_reflect_rdotl(in,out)==
        fixed_local_specular_reflect_emit_result::exact);
    CHECK(out.result_temp_register==3u);
    CHECK(out.result_component==2u);

    // DP3 r3.z, N, V
    CHECK(out.words[0]==0x07002010u);
    CHECK(out.words[1]==0x00100042u);
    CHECK(out.words[2]==3u);
    CHECK(out.words[3]==0x00100246u);
    CHECK(out.words[4]==5u);
    CHECK(out.words[5]==0x00100246u);
    CHECK(out.words[6]==6u);

    // DP3 r3.w, V, L
    CHECK(out.words[7]==0x07002010u);
    CHECK(out.words[8]==0x00100082u);
    CHECK(out.words[9]==3u);
    CHECK(out.words[10]==0x00100246u);
    CHECK(out.words[11]==6u);
    CHECK(out.words[12]==0x00100246u);
    CHECK(out.words[13]==10u);

    // DP3 r4.w, N, L
    CHECK(out.words[14]==0x07002010u);
    CHECK(out.words[15]==0x00100082u);
    CHECK(out.words[16]==4u);

    // Final MAD: r3.z = r3.w * (-1) + r3.z.
    CHECK(out.words[35]==0x09000032u);
    CHECK(out.words[36]==0x00100042u);
    CHECK(out.words[37]==3u);
    CHECK(out.words[38]==0x0010003au);
    CHECK(out.words[39]==3u);
    CHECK(out.words[40]==0x00004001u);
    CHECK(out.words[41]==0xbf800000u);
    CHECK(out.words[42]==0x0010002au);
    CHECK(out.words[43]==3u);

    auto alias=in;
    alias.auxiliary_scalar_dst=alias.primary_scalar_dst;
    CHECK(emit_fixed_local_specular_reflect_rdotl(alias,out)==
        fixed_local_specular_reflect_emit_result::fail_aliasing_destination);

    std::cout<<"fixed_local_specular_reflect_lowering_tests: PASS\n";
    return 0;
}
