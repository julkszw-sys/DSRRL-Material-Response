#include "dsrrl/runtime/mr_dxbc_transform.hpp"
#include "dsrrl/sha256.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <utility>

namespace dsrrl::runtime::mr {
namespace {

constexpr std::uint32_t k_mask = 0xffffffffu;
constexpr std::array<std::uint32_t,4> k_cb12_decl = {
    0x04000059u, 0x00208e46u, 0x0000000cu, 0x00000004u
};

constexpr std::array<std::uint32_t,4> k_t1_resource_decl = {
    0x00005555u, 0x04001858u, 0x00107000u, 0x00000001u
};

constexpr char k_specrgb_rdef_name[] = "DSRRL_PTDE_SPEC_RGB";

std::uint32_t read_u32(const std::uint8_t *p) noexcept
{
    std::uint32_t v = 0;
    std::memcpy(&v, p, sizeof(v));
    return v;
}

void write_u32(std::uint8_t *p, std::uint32_t v) noexcept
{
    std::memcpy(p, &v, sizeof(v));
}

std::uint32_t rol(std::uint32_t x, std::uint32_t n) noexcept
{
    return (x << n) | (x >> (32u - n));
}
std::uint32_t ff(std::uint32_t x,std::uint32_t y,std::uint32_t z) noexcept { return (x&y)|((~x)&z); }
std::uint32_t gg(std::uint32_t x,std::uint32_t y,std::uint32_t z) noexcept { return (x&z)|(y&(~z)); }
std::uint32_t hh(std::uint32_t x,std::uint32_t y,std::uint32_t z) noexcept { return x^y^z; }
std::uint32_t ii(std::uint32_t x,std::uint32_t y,std::uint32_t z) noexcept { return y^(x|(~z)); }

using md_fn = std::uint32_t(*)(std::uint32_t,std::uint32_t,std::uint32_t);
struct md_op { md_fn fn; std::uint8_t k; std::uint8_t s; std::uint32_t ac; };

constexpr std::array<md_op,64> k_ops = {{
{ff,0,7,3614090360u},{ff,1,12,3905402710u},{ff,2,17,606105819u},{ff,3,22,3250441966u},
{ff,4,7,4118548399u},{ff,5,12,1200080426u},{ff,6,17,2821735955u},{ff,7,22,4249261313u},
{ff,8,7,1770035416u},{ff,9,12,2336552879u},{ff,10,17,4294925233u},{ff,11,22,2304563134u},
{ff,12,7,1804603682u},{ff,13,12,4254626195u},{ff,14,17,2792965006u},{ff,15,22,1236535329u},
{gg,1,5,4129170786u},{gg,6,9,3225465664u},{gg,11,14,643717713u},{gg,0,20,3889429448u},
{gg,5,5,3593408605u},{gg,10,9,38016083u},{gg,15,14,3634488961u},{gg,4,20,3889429448u},
{gg,9,5,568446438u},{gg,14,9,3275163606u},{gg,3,14,4107603335u},{gg,8,20,1163531501u},
{gg,13,5,2850285829u},{gg,2,9,4243563512u},{gg,7,14,1735328473u},{gg,12,20,2368359562u},
{hh,5,4,4294588738u},{hh,8,11,2272392833u},{hh,11,16,1839030562u},{hh,14,23,4259657740u},
{hh,1,4,2763975236u},{hh,4,11,1272893353u},{hh,7,16,4139469664u},{hh,10,23,3200236656u},
{hh,13,4,681279174u},{hh,0,11,3936430074u},{hh,3,16,3572445317u},{hh,6,23,76029189u},
{hh,9,4,3654602809u},{hh,12,11,3873151461u},{hh,15,16,530742520u},{hh,2,23,3299628645u},
{ii,0,6,4096336452u},{ii,7,10,1126891415u},{ii,14,15,2878612391u},{ii,5,21,4237533241u},
{ii,12,6,1700485571u},{ii,3,10,2399980690u},{ii,10,15,4293915773u},{ii,1,21,2240044497u},
{ii,8,6,1873313359u},{ii,15,10,4264355552u},{ii,6,15,2734768916u},{ii,13,21,1309151649u},
{ii,4,6,4149444226u},{ii,11,10,3174756917u},{ii,2,15,718787259u},{ii,9,21,3951481745u}
}};

using md_state = std::array<std::uint32_t,4>;
using md_block = std::array<std::uint32_t,16>;

md_state md_transform(md_state state, const md_block &x) noexcept
{
    auto a=state[0],b=state[1],c=state[2],d=state[3];
    auto step=[](md_fn fn,std::uint32_t aa,std::uint32_t bb,std::uint32_t cc,std::uint32_t dd,
                 std::uint32_t xx,std::uint32_t ss,std::uint32_t ac) noexcept {
        aa = aa + fn(bb,cc,dd) + xx + ac;
        aa = rol(aa,ss);
        return aa + bb;
    };
    for(std::size_t j=0;j<k_ops.size();++j){
        const auto &op=k_ops[j];
        switch(j&3u){
        case 0: a=step(op.fn,a,b,c,d,x[op.k],op.s,op.ac); break;
        case 1: d=step(op.fn,d,a,b,c,x[op.k],op.s,op.ac); break;
        case 2: c=step(op.fn,c,d,a,b,x[op.k],op.s,op.ac); break;
        default:b=step(op.fn,b,c,d,a,x[op.k],op.s,op.ac); break;
        }
    }
    return {state[0]+a,state[1]+b,state[2]+c,state[3]+d};
}

std::array<std::uint8_t,16> dxbc_checksum(std::span<const std::uint8_t> data)
{
    std::array<std::uint8_t,16> out{};
    if(data.size()<0x20) return out;
    const auto p=data.subspan(0x14);
    const std::uint32_t nbits=static_cast<std::uint32_t>(p.size()*8u);
    md_state st={0x67452301u,0xefcdab89u,0x98badcfeu,0x10325476u};
    const std::size_t full=p.size()&~std::size_t(63);
    for(std::size_t off=0;off<full;off+=64){
        md_block block{};
        for(std::size_t i=0;i<16;++i) block[i]=read_u32(p.data()+off+i*4);
        st=md_transform(st,block);
    }
    const std::size_t rem=p.size()-full;
    if(rem>=56){
        std::array<std::uint8_t,64> raw{};
        if(rem) std::memcpy(raw.data(),p.data()+full,rem);
        raw[rem]=0x80;
        md_block block{};
        for(std::size_t i=0;i<16;++i) block[i]=read_u32(raw.data()+i*4);
        st=md_transform(st,block);
        md_block tail{}; tail[0]=nbits; tail[15]=(nbits>>2)|1u;
        st=md_transform(st,tail);
    }else{
        std::array<std::uint8_t,64> raw{};
        write_u32(raw.data(),nbits);
        if(rem) std::memcpy(raw.data()+4,p.data()+full,rem);
        raw[4+rem]=0x80;
        write_u32(raw.data()+60,(nbits>>2)|1u);
        md_block block{};
        for(std::size_t i=0;i<16;++i) block[i]=read_u32(raw.data()+i*4);
        st=md_transform(st,block);
    }
    for(std::size_t i=0;i<4;++i) write_u32(out.data()+i*4,st[i]);
    return out;
}

std::string sha_hex(std::span<const std::uint8_t> bytes)
{
    const auto *p=reinterpret_cast<const std::byte*>(bytes.data());
    return dsrrl::to_hex(dsrrl::sha256({p,bytes.size()}));
}

struct chunk { std::array<char,4> tag{}; std::vector<std::uint8_t> payload; };

bool parse_dxbc(std::span<const std::uint8_t> d,std::vector<chunk> &chunks,std::string &err)
{
    if(d.size()<32 || std::memcmp(d.data(),"DXBC",4)!=0){err="DXBC magic/header";return false;}
    if(read_u32(d.data()+24)!=d.size()){err="DXBC total size";return false;}
    const std::uint32_t n=read_u32(d.data()+28);
    if(n==0 || n>64 || 32ull+4ull*n>d.size()){err="DXBC chunk count";return false;}
    chunks.clear(); chunks.reserve(n);
    for(std::uint32_t i=0;i<n;++i){
        const std::uint32_t off=read_u32(d.data()+32+i*4);
        if(off+8ull>d.size()){err="DXBC chunk offset";return false;}
        const std::uint32_t sz=read_u32(d.data()+off+4);
        if(off+8ull+sz>d.size()){err="DXBC chunk size";return false;}
        chunk c{}; std::memcpy(c.tag.data(),d.data()+off,4);
        c.payload.assign(d.begin()+off+8,d.begin()+off+8+sz);
        chunks.push_back(std::move(c));
    }
    return true;
}

std::vector<std::uint8_t> rebuild(std::span<const std::uint8_t> original,
                                  const std::vector<chunk> &chunks,
                                  std::string &err)
{
    const std::size_t header=32+4*chunks.size();
    if(original.size()<header){err="DXBC original header";return{};}
    std::vector<std::uint8_t> out(original.begin(),original.begin()+header);
    std::vector<std::uint32_t> offsets; offsets.reserve(chunks.size());
    for(const auto &c:chunks){
        if(out.size()>std::numeric_limits<std::uint32_t>::max()){err="DXBC too large";return{};}
        offsets.push_back(static_cast<std::uint32_t>(out.size()));
        out.insert(out.end(),reinterpret_cast<const std::uint8_t*>(c.tag.data()),
                   reinterpret_cast<const std::uint8_t*>(c.tag.data())+4);
        const auto old=out.size(); out.resize(old+4); write_u32(out.data()+old,static_cast<std::uint32_t>(c.payload.size()));
        out.insert(out.end(),c.payload.begin(),c.payload.end());
    }
    write_u32(out.data()+24,static_cast<std::uint32_t>(out.size()));
    write_u32(out.data()+28,static_cast<std::uint32_t>(chunks.size()));
    for(std::size_t i=0;i<offsets.size();++i) write_u32(out.data()+32+i*4,offsets[i]);
    std::fill(out.begin()+4,out.begin()+20,std::uint8_t{0});
    const auto sum=dxbc_checksum(out);
    std::copy(sum.begin(),sum.end(),out.begin()+4);
    return out;
}

bool get_shex_words(std::span<const std::uint8_t> d,std::vector<chunk> &chunks,
                    std::size_t &shex,std::vector<std::uint32_t> &words,std::string &err)
{
    if(!parse_dxbc(d,chunks,err)) return false;
    for(std::size_t i=0;i<chunks.size();++i){
        if(std::memcmp(chunks[i].tag.data(),"SHEX",4)==0){
            if((chunks[i].payload.size()%4)!=0){err="SHEX alignment";return false;}
            shex=i; words.resize(chunks[i].payload.size()/4);
            for(std::size_t w=0;w<words.size();++w) words[w]=read_u32(chunks[i].payload.data()+w*4);
            return true;
        }
    }
    err="SHEX missing"; return false;
}

std::vector<std::uint8_t> rebuild_words(std::span<const std::uint8_t> base,
                                        std::vector<chunk> chunks,std::size_t shex,
                                        const std::vector<std::uint32_t> &words,std::string &err)
{
    chunks[shex].payload.resize(words.size()*4);
    for(std::size_t i=0;i<words.size();++i) write_u32(chunks[shex].payload.data()+i*4,words[i]);
    return rebuild(base,chunks,err);
}

bool hash_is(std::span<const std::uint8_t> data,std::string_view expected)
{
    return sha_hex(data)==expected;
}

bool patch_rdef_specrgb(std::vector<chunk> &chunks,std::string &err)
{
    std::size_t rdef_index=chunks.size();
    for(std::size_t i=0;i<chunks.size();++i){
        if(std::memcmp(chunks[i].tag.data(),"RDEF",4)==0){
            if(rdef_index!=chunks.size()){err="RDEF duplicate";return false;}
            rdef_index=i;
        }
    }
    if(rdef_index==chunks.size()){err="RDEF missing";return false;}

    auto &rdef=chunks[rdef_index].payload;
    if(rdef.size()<16u){err="RDEF header";return false;}

    const std::uint32_t binding_count=read_u32(rdef.data()+8u);
    const std::uint32_t binding_offset=read_u32(rdef.data()+12u);
    constexpr std::uint32_t binding_size=32u;

    if(binding_count==0u ||
       binding_count>256u ||
       binding_offset>rdef.size() ||
       static_cast<std::uint64_t>(binding_count)*binding_size>
           rdef.size()-binding_offset){
        err="RDEF binding table"; return false;
    }

    std::uint32_t t1_offset=0u;
    std::uint32_t t1_count=0u;
    for(std::uint32_t i=0;i<binding_count;++i){
        const std::uint32_t off=binding_offset+i*binding_size;
        const std::uint32_t type=read_u32(rdef.data()+off+4u);
        const std::uint32_t bind_point=read_u32(rdef.data()+off+20u);
        const std::uint32_t bind_count=read_u32(rdef.data()+off+24u);
        if(type==2u && bind_point==10u){
            err="RDEF t10 already declared"; return false;
        }
        if(type==2u && bind_point==1u && bind_count==1u){
            t1_offset=off; ++t1_count;
        }
    }
    if(t1_count!=1u){err="RDEF exact t1 texture binding";return false;}

    const std::uint32_t name_offset=
        static_cast<std::uint32_t>(rdef.size());
    rdef.insert(
        rdef.end(),
        reinterpret_cast<const std::uint8_t*>(k_specrgb_rdef_name),
        reinterpret_cast<const std::uint8_t*>(k_specrgb_rdef_name)+sizeof(k_specrgb_rdef_name));
    while((rdef.size()&3u)!=0u)
        rdef.push_back(0u);

    const std::uint32_t new_table_offset=
        static_cast<std::uint32_t>(rdef.size());

    const auto old_table_begin=rdef.begin()+binding_offset;
    const auto old_table_end=old_table_begin+
        static_cast<std::ptrdiff_t>(binding_count*binding_size);
    std::vector<std::uint8_t> table(old_table_begin,old_table_end);

    std::array<std::uint8_t,binding_size> t10{};
    std::copy_n(
        rdef.begin()+t1_offset,
        binding_size,
        t10.begin());
    write_u32(t10.data()+0u,name_offset);
    write_u32(t10.data()+20u,10u);
    table.insert(table.end(),t10.begin(),t10.end());
    rdef.insert(rdef.end(),table.begin(),table.end());

    write_u32(rdef.data()+8u,binding_count+1u);
    write_u32(rdef.data()+12u,new_table_offset);
    return true;
}

std::vector<std::uint8_t> patch_specrgb(
    std::span<const std::uint8_t> base,
    const plan &p,
    std::string &err)
{
    std::vector<chunk> chunks;
    if(!parse_dxbc(base,chunks,err))
        return {};

    std::size_t shex=chunks.size();
    for(std::size_t i=0;i<chunks.size();++i){
        if(std::memcmp(chunks[i].tag.data(),"SHEX",4)==0){
            if(shex!=chunks.size()){err="SHEX duplicate";return{};}
            shex=i;
        }
    }
    if(shex==chunks.size()){err="SHEX missing";return{};}
    if((chunks[shex].payload.size()%4u)!=0u){err="SHEX alignment";return{};}

    std::vector<std::uint32_t> words(chunks[shex].payload.size()/4u);
    for(std::size_t i=0;i<words.size();++i)
        words[i]=read_u32(chunks[shex].payload.data()+i*4u);

    if(words.size()<12u || words[1]!=words.size()){
        err="SpecRGB SHEX length token"; return{};
    }

    const std::size_t dcl=p.spec_dcl_t1_word;
    const std::size_t sample=p.spec_sample_t1_word;
    if(dcl+4u>words.size() || sample+11u>words.size() || dcl>=sample){
        err="SpecRGB site range"; return{};
    }

    for(std::size_t i=0;i<k_t1_resource_decl.size();++i){
        if(words[dcl+i]!=k_t1_resource_decl[i]){
            err="SpecRGB exact t1 declaration"; return{};
        }
    }

    if(words[sample+0u]!=0x8b000045u ||
       words[sample+1u]!=0x800000c2u ||
       words[sample+2u]!=0x00155543u ||
       words[sample+8u]!=1u ||
       words[sample+3u]<0x10u){
        err="SpecRGB exact t1 sample"; return{};
    }

    std::array<std::uint32_t,4> t10_dcl{};
    std::copy_n(words.begin()+static_cast<std::ptrdiff_t>(dcl),4u,t10_dcl.begin());
    t10_dcl[3]=10u;

    std::array<std::uint32_t,11> t10_sample{};
    std::copy_n(words.begin()+static_cast<std::ptrdiff_t>(sample),11u,t10_sample.begin());
    t10_sample[3]-=0x10u;
    t10_sample[8]=10u;

    words.insert(
        words.begin()+static_cast<std::ptrdiff_t>(dcl+4u),
        t10_dcl.begin(),
        t10_dcl.end());

    const std::size_t shifted_sample=sample+4u;
    words.insert(
        words.begin()+static_cast<std::ptrdiff_t>(shifted_sample+11u),
        t10_sample.begin(),
        t10_sample.end());

    words[1]+=15u;

    chunks[shex].payload.resize(words.size()*4u);
    for(std::size_t i=0;i<words.size();++i)
        write_u32(chunks[shex].payload.data()+i*4u,words[i]);

    if(!patch_rdef_specrgb(chunks,err))
        return {};

    return rebuild(base,chunks,err);
}

} // namespace

const plan *find_plan(std::size_t size,std::string_view sha256) noexcept
{
    for(const auto &p:k_plans)
        if(p.stock_size==size && p.original_sha256==sha256) return &p;
    return nullptr;
}

transform_result transform(std::span<const std::uint8_t> stock,const plan &p,variant v)
{
    transform_result r;
    if(stock.size()!=p.stock_size){r.error="stock size mismatch";return r;}
    if(!hash_is(stock,p.original_sha256)){r.error="stock SHA mismatch";return r;}

    std::vector<chunk> chunks; std::vector<std::uint32_t> words; std::size_t shex=0;
    if(!get_shex_words(stock,chunks,shex,words,r.error)) return r;
    if(words.size()<12 || words[1]!=words.size()){r.error="SHEX length token";return r;}
    for(const auto site:p.cb_sites){
        if(site+1>=words.size() || words[site]!=0u || words[site+1]!=9u){r.error="V29 cb0[9] site";return r;}
    }
    if(p.pow_site+2>=words.size() ||
       words[p.pow_site]!=0x400ccccdu || words[p.pow_site+1]!=0x400ccccdu || words[p.pow_site+2]!=0x400ccccdu){
        r.error="V29 diffuse pow site";return r;
    }
    for(const auto site:p.cb_sites){words[site]=12u;words[site+1]=1u;}
    words[p.pow_site]=words[p.pow_site+1]=words[p.pow_site+2]=0x3f800000u;
    words.insert(words.begin()+11,k_cb12_decl.begin(),k_cb12_decl.end());
    words[1]+=4u;
    auto v29=rebuild_words(stock,chunks,shex,words,r.error);
    if(v29.empty() || !hash_is(v29,p.v29_sha256)){r.error="V29 output SHA";return r;}

    const bool want_spec =
        v==variant::diffuse_v29_specrgb ||
        v==variant::full_v211_specrgb;

    if(v==variant::diffuse_v29){
        r.ok=true; r.code=std::move(v29); return r;
    }

    if(v==variant::diffuse_v29_specrgb){
        auto spec=patch_specrgb(v29,p,r.error);
        if(spec.empty() ||
           spec.size()!=p.specrgb_replacement_size ||
           !hash_is(spec,p.v29_specrgb_sha256)){
            r.error="V29 SpecRGB output SHA"; return r;
        }
        r.ok=true; r.code=std::move(spec); return r;
    }

    if(!get_shex_words(v29,chunks,shex,words,r.error)) return r;
    for(const auto &patch:p.v210){
        if(patch.word>=words.size() || words[patch.word]!=patch.old_value){r.error="V210 patch precondition";return r;}
        words[patch.word]=patch.new_value;
    }
    auto v210=rebuild_words(v29,chunks,shex,words,r.error);
    if(v210.empty() || !hash_is(v210,p.v210_sha256)){r.error="V210 output SHA";return r;}

    if(!get_shex_words(v210,chunks,shex,words,r.error)) return r;
    if(p.v211.word>=words.size() || words[p.v211.word]!=p.v211.old_value){r.error="V211 patch precondition";return r;}
    words[p.v211.word]=p.v211.new_value;
    auto v211=rebuild_words(v210,chunks,shex,words,r.error);
    if(v211.empty() || v211.size()!=p.replacement_size || !hash_is(v211,p.v211_sha256)){
        r.error="V211 output SHA";return r;
    }

    if(want_spec){
        auto spec=patch_specrgb(v211,p,r.error);
        if(spec.empty() ||
           spec.size()!=p.specrgb_replacement_size ||
           !hash_is(spec,p.v211_specrgb_sha256)){
            r.error="V211 SpecRGB output SHA"; return r;
        }
        r.ok=true; r.code=std::move(spec); return r;
    }

    r.ok=true; r.code=std::move(v211); return r;
}

} // namespace dsrrl::runtime::mr
