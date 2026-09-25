#include "dsrrl/runtime/mr_dxbc_transform.hpp"
#include "dsrrl/runtime/pmetal_envspec_rgba_authority.hpp"

#include <array>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

using namespace dsrrl::runtime::mr;

#define CHECK(x) do { if(!(x)){ std::cerr<<"CHECK FAILED: "<<#x<<" line "<<__LINE__<<"\n"; return 1; } } while(false)

namespace {

std::uint32_t read_u32(const std::uint8_t *p)
{
    std::uint32_t v=0;
    std::memcpy(&v,p,sizeof(v));
    return v;
}

void append_u32(std::vector<std::uint8_t> &out,std::uint32_t v)
{
    const auto old=out.size();
    out.resize(old+4u);
    std::memcpy(out.data()+old,&v,sizeof(v));
}

void write_u32(std::vector<std::uint8_t> &out,std::size_t off,std::uint32_t v)
{
    std::memcpy(out.data()+off,&v,sizeof(v));
}

std::vector<std::uint8_t> make_spec_fixture()
{
    const std::array<std::uint32_t,17> shex={{
        0u,17u,
        0x04001858u,0x00107000u,1u,0x00005555u,
        0x8b000045u,0x800000c2u,0x00155543u,0x001000f2u,
        0u,0u,0u,0u,1u,0u,0u
    }};

    std::vector<std::uint8_t> rdef(48u,0u);
    write_u32(rdef,8u,1u);
    write_u32(rdef,12u,16u);
    write_u32(rdef,16u+4u,2u);
    write_u32(rdef,16u+20u,1u);
    write_u32(rdef,16u+24u,1u);

    std::vector<std::uint8_t> shex_bytes;
    shex_bytes.reserve(shex.size()*4u);
    for(const auto w:shex)
        append_u32(shex_bytes,w);

    constexpr std::uint32_t chunk_count=2u;
    const std::uint32_t header_size=32u+4u*chunk_count;
    const std::uint32_t rdef_offset=header_size;
    const std::uint32_t shex_offset=rdef_offset+8u+
        static_cast<std::uint32_t>(rdef.size());
    const std::uint32_t total_size=shex_offset+8u+
        static_cast<std::uint32_t>(shex_bytes.size());

    std::vector<std::uint8_t> out(header_size,0u);
    std::memcpy(out.data(),"DXBC",4u);
    write_u32(out,24u,total_size);
    write_u32(out,28u,chunk_count);
    write_u32(out,32u,rdef_offset);
    write_u32(out,36u,shex_offset);

    out.insert(out.end(),{'R','D','E','F'});
    append_u32(out,static_cast<std::uint32_t>(rdef.size()));
    out.insert(out.end(),rdef.begin(),rdef.end());

    out.insert(out.end(),{'S','H','E','X'});
    append_u32(out,static_cast<std::uint32_t>(shex_bytes.size()));
    out.insert(out.end(),shex_bytes.begin(),shex_bytes.end());
    return out;
}

bool inspect_spec_fixture(const std::vector<std::uint8_t> &dxbc)
{
    if(dxbc.size()<40u || std::memcmp(dxbc.data(),"DXBC",4u)!=0)
        return false;

    const auto count=read_u32(dxbc.data()+28u);
    std::size_t t1_dcl=0,t10_dcl=0,t1_sample=0,t10_sample=0;
    bool rdef_ok=false;

    for(std::uint32_t i=0;i<count;++i){
        const auto off=read_u32(dxbc.data()+32u+i*4u);
        if(off+8u>dxbc.size())
            return false;
        const auto size=read_u32(dxbc.data()+off+4u);
        if(off+8u+size>dxbc.size())
            return false;

        const auto *tag=dxbc.data()+off;
        const auto *payload=dxbc.data()+off+8u;

        if(std::memcmp(tag,"SHEX",4u)==0){
            if((size%4u)!=0u)
                return false;
            const auto words=size/4u;
            std::size_t at=2u;
            while(at<words){
                const auto token=read_u32(payload+at*4u);
                const auto len=(token>>24u)&0x7fu;
                const auto opcode=token&0x7ffu;
                if(len==0u || at+len>words)
                    return false;

                if(opcode==0x58u && len==4u){
                    const auto bind=read_u32(payload+(at+2u)*4u);
                    if(bind==1u) ++t1_dcl;
                    if(bind==10u) ++t10_dcl;
                }
                if(opcode>=0x45u && opcode<=0x4au && len==11u){
                    const auto bind=read_u32(payload+(at+8u)*4u);
                    if(bind==1u) ++t1_sample;
                    if(bind==10u) ++t10_sample;
                }
                at+=len;
            }
        }

        if(std::memcmp(tag,"RDEF",4u)==0){
            if(size<16u)
                return false;
            const auto bindings=read_u32(payload+8u);
            const auto table=read_u32(payload+12u);
            if(bindings!=2u || table>size ||
               static_cast<std::uint64_t>(bindings)*32u>size-table)
                return false;

            bool have_t1=false,have_t10=false;
            for(std::uint32_t b=0;b<bindings;++b){
                const auto entry=table+b*32u;
                const auto type=read_u32(payload+entry+4u);
                const auto point=read_u32(payload+entry+20u);
                const auto n=read_u32(payload+entry+24u);
                if(type==2u && n==1u && point==1u) have_t1=true;
                if(type==2u && n==1u && point==10u) have_t10=true;
            }
            rdef_ok=have_t1 && have_t10;
        }
    }

    return rdef_ok &&
        t1_dcl==1u && t10_dcl==1u &&
        t1_sample==1u && t10_sample==1u;
}

} // namespace

int main()
{
    CHECK(k_plans.size()==24u);
    for(std::size_t i=0;i<k_plans.size();++i){
        CHECK(k_plans[i].index==i);
        CHECK(k_plans[i].stock_size>16000u);
        CHECK(k_plans[i].replacement_size==k_plans[i].stock_size+16u);
        CHECK(k_plans[i].cb_sites[0]<k_plans[i].cb_sites[1]);
        CHECK(k_plans[i].original_sha256.size()==64u);
        CHECK(k_plans[i].v29_sha256.size()==64u);
        CHECK(k_plans[i].v210_sha256.size()==64u);
        CHECK(k_plans[i].v211_sha256.size()==64u);
        CHECK(find_plan(k_plans[i].stock_size,k_plans[i].original_sha256)==&k_plans[i]);
    }
    CHECK(find_plan(0,"nope")==nullptr);

    CHECK(pmetal_envspec_rgba_authority::k_entries.size()==3u);
    CHECK(pmetal_envspec_rgba_authority::find(
        "70b85d49cea116ff1f72a3fd5bb7726ee72aee5a0655cc81718a83a659ee03f5")!=nullptr);
    CHECK(pmetal_envspec_rgba_authority::k_entries[0].reflection_coord_register==7u);
    CHECK(pmetal_envspec_rgba_authority::k_entries[1].reflection_coord_register==6u);
    CHECK(pmetal_envspec_rgba_authority::k_entries[2].reflection_coord_register==5u);
    CHECK(pmetal_envspec_rgba_authority::k_entries[0].merge_word-
          pmetal_envspec_rgba_authority::k_entries[0].t12_word==115u);

    const std::uint8_t junk[32]={};
    const auto r=transform(junk,k_plans[0],variant::full_v211);
    CHECK(!r.ok);
    CHECK(!r.error.empty());

    const auto fixture=make_spec_fixture();
    const auto spec=transform_spec_rgb(fixture);
    CHECK(spec.ok);
    CHECK(spec.code.size()>fixture.size());
    CHECK(inspect_spec_fixture(spec.code));

    std::cout<<"dsrrl_runtime_v1_tests: PASS\n";
    return 0;
}
