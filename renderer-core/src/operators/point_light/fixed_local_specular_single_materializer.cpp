#include "dsrrl/operators/point_light/fixed_local_specular_single_materializer.hpp"

#include "dsrrl/operators/legacy_plan/a1_create_time_materializer.hpp"
#include "dsrrl/operators/legacy_plan/dxbc_checksum.hpp"
#include "dsrrl/operators/point_light/fixed_local_geometry_contract.hpp"
#include "dsrrl/operators/point_light/fixed_local_specular_material_samples.hpp"
#include "dsrrl/operators/point_light/fixed_local_specular_t19_lowering.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <utility>
#include <vector>

namespace dsrrl::operators::point_light {
namespace {

using legacy_plan::dxbc::read_u32;
using legacy_plan::dxbc::write_u32;

constexpr std::uint16_t k_op_add = 0u;
constexpr std::uint16_t k_op_div = 14u;
constexpr std::uint16_t k_op_endif = 21u;
constexpr std::uint16_t k_op_endswitch = 23u;
constexpr std::uint16_t k_op_customdata = 53u;
constexpr std::uint16_t k_op_mov = 54u;
constexpr std::uint16_t k_op_mul = 56u;
constexpr std::uint16_t k_op_dcl_resource = 88u;
constexpr std::uint16_t k_op_dcl_cb = 89u;
constexpr std::uint16_t k_op_dcl_temps = 104u;
constexpr std::uint16_t k_op_dcl_structured = 162u;
constexpr std::uint16_t k_op_ld_structured = 167u;

constexpr std::uint32_t k_temp_dst_x = 0x00100012u;
constexpr std::uint32_t k_temp_dst_y = 0x00100022u;
constexpr std::uint32_t k_temp_dst_xyz = 0x00100072u;
constexpr std::uint32_t k_temp_src_x = 0x0010000au;
constexpr std::uint32_t k_temp_src_xyz = 0x00100246u;
constexpr std::uint32_t k_temp_src_yzw = 0x00100796u;
constexpr std::uint32_t k_input_src_xyz = 0x00101246u;
constexpr std::uint32_t k_cb_src_xyz = 0x00208246u;
constexpr std::uint32_t k_imm_scalar = 0x00004001u;
constexpr std::uint32_t k_imm_vector = 0x00004002u;

constexpr std::array<std::uint32_t,4> k_b12_decl = {
    0x04000059u,0x00208e46u,12u,4u
};

struct chunk {
    std::array<char,4> tag{};
    std::vector<std::uint8_t> payload;
};

struct instruction {
    std::uint32_t start=0u;
    std::uint32_t end=0u;
    std::uint16_t opcode=0u;
};

struct insertion {
    std::uint32_t word=0u;
    std::vector<std::uint32_t> payload;
};

bool parse(
    const std::uint8_t *source,
    std::size_t size,
    std::vector<chunk> &chunks,
    std::size_t &code_index,
    std::vector<std::uint32_t> &words) noexcept
{
    if (source==nullptr || size<32u ||
        !legacy_plan::dxbc::checksum_container_valid(source,size))
        return false;

    const auto count=read_u32(source+28u);
    if(count==0u || count>64u || 32ull+4ull*count>size)
        return false;

    chunks.clear();
    code_index=static_cast<std::size_t>(-1);
    try { chunks.reserve(count); } catch(...) { return false; }

    for(std::uint32_t i=0u;i<count;++i) {
        const auto off=static_cast<std::size_t>(
            read_u32(source+32u+i*4u));
        if(off>size || size-off<8u) return false;
        const auto payload_size=static_cast<std::size_t>(
            read_u32(source+off+4u));
        if(payload_size>size-off-8u) return false;

        chunk c{};
        std::memcpy(c.tag.data(),source+off,4u);
        try {
            c.payload.assign(
                source+off+8u,
                source+off+8u+payload_size);
        } catch(...) { return false; }

        if(std::memcmp(c.tag.data(),"SHEX",4u)==0 ||
           std::memcmp(c.tag.data(),"SHDR",4u)==0) {
            if(code_index!=static_cast<std::size_t>(-1) ||
               (payload_size&3u)!=0u)
                return false;
            code_index=chunks.size();
        }
        chunks.push_back(std::move(c));
    }

    if(code_index==static_cast<std::size_t>(-1))
        return false;

    const auto &payload=chunks[code_index].payload;
    try { words.resize(payload.size()/4u); }
    catch(...) { return false; }

    for(std::size_t i=0u;i<words.size();++i)
        words[i]=read_u32(payload.data()+i*4u);

    return words.size()>=3u && words[1]==words.size();
}

bool decode(
    const std::vector<std::uint32_t> &words,
    std::vector<instruction> &out) noexcept
{
    out.clear();
    std::size_t at=2u;
    try { out.reserve(words.size()/4u); }
    catch(...) { return false; }

    while(at<words.size()) {
        const auto opcode=
            static_cast<std::uint16_t>(words[at]&0x7ffu);
        std::uint32_t length=0u;
        if(opcode==k_op_customdata) {
            if(at+1u>=words.size()) return false;
            length=words[at+1u];
        } else {
            length=(words[at]>>24u)&0x7fu;
        }
        if(length==0u || length>words.size()-at)
            return false;
        out.push_back({
            static_cast<std::uint32_t>(at),
            static_cast<std::uint32_t>(at+length),
            opcode});
        at+=length;
    }
    return at==words.size();
}

const instruction *find_instruction(
    const std::vector<instruction> &instructions,
    std::uint32_t word) noexcept
{
    for(const auto &i:instructions)
        if(i.start==word) return &i;
    return nullptr;
}

bool has_cb_index(
    const std::vector<std::uint32_t> &words,
    const instruction &ins,
    std::uint32_t index) noexcept
{
    for(std::uint32_t w=ins.start+1u;w+2u<ins.end;++w) {
        const auto token=words[w];
        if(((token>>12u)&0xffu)==8u &&
           ((token>>20u)&0x3u)==2u &&
           words[w+1u]==0u &&
           words[w+2u]==index)
            return true;
        // Extended NEG constant-buffer operands retain type/dimension in the
        // base token; the following DWORD is the modifier extension.
        if((token&0x80000000u)!=0u &&
           ((token>>12u)&0xffu)==8u &&
           ((token>>20u)&0x3u)==2u &&
           w+3u<ins.end &&
           words[w+2u]==0u &&
           words[w+3u]==index)
            return true;
    }
    return false;
}

bool parse_color0(
    const std::vector<chunk> &chunks,
    std::uint32_t &reg) noexcept
{
    bool hit=false;
    for(const auto &c:chunks) {
        if(std::memcmp(c.tag.data(),"ISGN",4u)!=0)
            continue;
        if(c.payload.size()<8u) return false;
        const auto count=read_u32(c.payload.data());
        if(count==0u || count>64u ||
           8ull+24ull*count>c.payload.size())
            return false;

        for(std::uint32_t i=0u;i<count;++i) {
            const auto at=8u+24u*i;
            const auto name_off=read_u32(c.payload.data()+at);
            const auto semantic_index=read_u32(c.payload.data()+at+4u);
            if(name_off>=c.payload.size()) return false;

            const char *name=
                reinterpret_cast<const char *>(c.payload.data()+name_off);
            const auto remaining=c.payload.size()-name_off;
            bool terminated=false;
            for(std::size_t n=0u;n<remaining;++n)
                if(name[n]=='\0') { terminated=true; break; }
            if(!terminated) return false;

            if(std::strcmp(name,"COLOR")==0 && semantic_index==0u) {
                if(hit) return false;
                reg=read_u32(c.payload.data()+at+16u);
                hit=true;
            }
        }
    }
    return hit;
}

std::uint8_t scalar_component(std::uint32_t dst_token) noexcept
{
    const auto mask=(dst_token>>4u)&0xfu;
    if(mask==1u) return 0u;
    if(mask==2u) return 1u;
    if(mask==4u) return 2u;
    if(mask==8u) return 3u;
    return 0xffu;
}

std::uint32_t temp_dst_scalar(std::uint8_t c) noexcept
{
    return c==0u?0x00100012u:
           c==1u?0x00100022u:
           c==2u?0x00100042u:
                 0x00100082u;
}

std::uint32_t temp_src_scalar(std::uint8_t c) noexcept
{
    return 0x0010000au+
        (static_cast<std::uint32_t>(c)<<4u);
}

void append(
    std::vector<std::uint32_t> &dst,
    const std::uint32_t *p,
    std::size_t n)
{
    dst.insert(dst.end(),p,p+n);
}

template<std::size_t N>
void append(
    std::vector<std::uint32_t> &dst,
    const std::array<std::uint32_t,N> &v)
{
    append(dst,v.data(),v.size());
}

void append_mul_vec_temp_cb(
    std::vector<std::uint32_t> &dst,
    std::uint32_t d,
    std::uint32_t s_token,
    std::uint32_t s,
    std::uint32_t cb_index)
{
    const std::uint32_t v[]={
        0x08000038u,k_temp_dst_xyz,d,
        s_token,s,
        k_cb_src_xyz,12u,cb_index
    };
    append(dst,v,8u);
}

void append_mul_vec_temp_input(
    std::vector<std::uint32_t> &dst,
    std::uint32_t d,
    std::uint32_t s,
    std::uint32_t input)
{
    const std::uint32_t v[]={
        0x07000038u,k_temp_dst_xyz,d,
        k_temp_src_xyz,s,
        k_input_src_xyz,input
    };
    append(dst,v,7u);
}

void append_mov_zero_xyz(
    std::vector<std::uint32_t> &dst,
    std::uint32_t d)
{
    const std::uint32_t v[]={
        0x08000036u,k_temp_dst_xyz,d,
        k_imm_vector,0u,0u,0u,0u
    };
    append(dst,v,8u);
}

void append_mov_scalar(
    std::vector<std::uint32_t> &dst,
    std::uint32_t d,
    std::uint32_t src_token,
    std::uint32_t s)
{
    const std::uint32_t v[]={
        0x05000036u,k_temp_dst_x,d,
        src_token,s
    };
    append(dst,v,5u);
}

void append_mul_vec_scalar(
    std::vector<std::uint32_t> &dst,
    std::uint32_t d,
    std::uint32_t vreg,
    std::uint32_t sreg,
    std::uint8_t component)
{
    const std::uint32_t v[]={
        0x07000038u,k_temp_dst_xyz,d,
        k_temp_src_xyz,vreg,
        temp_src_scalar(component),sreg
    };
    append(dst,v,7u);
}

void append_mad_accum(
    std::vector<std::uint32_t> &dst,
    std::uint32_t accum,
    std::uint32_t work,
    std::uint32_t material)
{
    const std::uint32_t v[]={
        0x09000032u,k_temp_dst_xyz,accum,
        k_temp_src_xyz,work,
        k_temp_src_xyz,material,
        k_temp_src_xyz,accum
    };
    append(dst,v,9u);
}

void append_max_scalar_zero(
    std::vector<std::uint32_t> &dst,
    const fixed_local_specular_operand_pair &pair)
{
    const auto component=scalar_component(pair.token[0]);
    const std::uint32_t v[]={
        0x07000034u,
        pair.token[0],pair.token[1],
        temp_src_scalar(component),pair.token[1],
        k_imm_scalar,0u
    };
    append(dst,v,7u);
}

void append_mul_accum_c139(
    std::vector<std::uint32_t> &dst,
    std::uint32_t accum)
{
    const std::uint32_t v[]={
        0x08000038u,k_temp_dst_xyz,accum,
        k_temp_src_xyz,accum,
        k_cb_src_xyz,0u,139u
    };
    append(dst,v,8u);
}

bool locate_t1_decl_and_temps(
    const std::vector<std::uint32_t> &words,
    const std::vector<instruction> &instructions,
    std::uint32_t &t1_decl_word,
    std::uint32_t &temps_word,
    std::uint32_t &temp_count) noexcept
{
    std::size_t t1_count=0u;
    std::size_t temps_count=0u;
    for(const auto &ins:instructions) {
        if(ins.opcode==k_op_dcl_resource &&
           ins.end-ins.start==4u &&
           words[ins.start+2u]==1u) {
            t1_decl_word=ins.start;
            ++t1_count;
        }
        if(ins.opcode==k_op_dcl_temps &&
           ins.end-ins.start==2u) {
            temps_word=ins.start;
            temp_count=words[ins.start+1u];
            ++temps_count;
        }
    }
    return t1_count==1u && temps_count==1u &&
           temp_count>0u && temp_count<4090u;
}

bool locate_c156_add(
    const std::vector<std::uint32_t> &words,
    const std::vector<instruction> &instructions,
    const fixed_local_specular_sample_site &t0,
    const instruction *&out) noexcept
{
    out=nullptr;
    for(std::size_t i=0u;i<instructions.size();++i) {
        if(instructions[i].start!=t0.instruction_word)
            continue;
        if(i+1u>=instructions.size()) return false;
        const auto &next=instructions[i+1u];
        if(next.opcode!=k_op_add ||
           next.end-next.start!=8u ||
           !has_cb_index(words,next,156u))
            return false;
        out=&next;
        return true;
    }
    return false;
}

bool locate_linear_template(
    const std::vector<std::uint32_t> &words,
    const std::vector<instruction> &instructions,
    std::uint32_t window_start,
    std::uint32_t window_end,
    std::uint32_t begin_cb,
    std::uint32_t end_cb,
    std::array<instruction,4> &out) noexcept
{
    std::size_t start_index=instructions.size();
    std::size_t end_index=instructions.size();
    for(std::size_t i=0u;i<instructions.size();++i) {
        if(instructions[i].start==window_start) start_index=i;
        if(instructions[i].end==window_end) end_index=i;
    }
    if(start_index==instructions.size() ||
       end_index==instructions.size() ||
       end_index<=start_index+4u)
        return false;

    std::size_t endswitch=instructions.size();
    for(std::size_t i=start_index;i<end_index;++i)
        if(instructions[i].opcode==k_op_endswitch)
            endswitch=i;
    if(endswitch==instructions.size())
        return false;

    for(std::size_t i=start_index;i+3u<endswitch;++i) {
        const auto &a=instructions[i];
        const auto &b=instructions[i+1u];
        const auto &c=instructions[i+2u];
        const auto &d=instructions[i+3u];
        if(a.opcode!=k_op_add || b.opcode!=k_op_add ||
           c.opcode!=k_op_div || d.opcode!=k_op_add ||
           a.end-a.start!=10u || b.end-b.start!=10u ||
           c.end-c.start!=7u || d.end-d.start!=8u ||
           (words[d.start]&0x2000u)==0u ||
           !has_cb_index(words,a,begin_cb) ||
           !has_cb_index(words,b,begin_cb) ||
           !has_cb_index(words,b,end_cb))
            continue;
        out={a,b,c,d};
        return true;
    }
    return false;
}

bool retarget_linear_template(
    const std::vector<std::uint32_t> &words,
    const std::array<instruction,4> &src,
    std::uint32_t work,
    std::uint32_t distance,
    std::vector<std::uint32_t> &out)
{
    out.clear();
    std::array<std::vector<std::uint32_t>,4> v;
    for(std::size_t i=0u;i<4u;++i)
        v[i].assign(
            words.begin()+src[i].start,
            words.begin()+src[i].end);

    // a: work.x = captured_distance - Begin
    v[0][1]=k_temp_dst_x; v[0][2]=work;
    v[0][3]=k_temp_src_x; v[0][4]=distance;

    // b: work.y = End - Begin; sources remain exact fixed cb operands.
    v[1][1]=k_temp_dst_y; v[1][2]=work;

    // c: work.x = work.x / work.y.
    v[2][1]=k_temp_dst_x; v[2][2]=work;
    v[2][3]=k_temp_src_x; v[2][4]=work;
    v[2][5]=temp_src_scalar(1u); v[2][6]=work;

    // d: work.x = sat(1 - work.x). Preserve exact NEG extension form.
    v[3][1]=k_temp_dst_x; v[3][2]=work;
    if(v[3][3]!=0x8010003au || v[3][4]!=0x00000041u)
        return false;
    v[3][3]=0x8010000au;
    v[3][5]=work;

    for(const auto &x:v) append(out,x.data(),x.size());
    return true;
}

bool strip_rdef(
    std::vector<chunk> &chunks,
    bool &stripped) noexcept
{
    stripped=false;
    auto it=std::remove_if(
        chunks.begin(),chunks.end(),
        [&](const chunk &c){
            if(std::memcmp(c.tag.data(),"RDEF",4u)==0) {
                stripped=true;
                return true;
            }
            return false;
        });
    chunks.erase(it,chunks.end());
    return stripped;
}

bool rebuild(
    const std::uint8_t *basis,
    std::size_t basis_size,
    std::vector<chunk> chunks,
    std::size_t old_code_index,
    const std::vector<std::uint32_t> &words,
    std::vector<std::uint8_t> &out) noexcept
{
    if(old_code_index>=chunks.size()) return false;

    // After RDEF removal, locate the sole code chunk again.
    std::size_t code_index=static_cast<std::size_t>(-1);
    for(std::size_t i=0u;i<chunks.size();++i)
        if(std::memcmp(chunks[i].tag.data(),"SHEX",4u)==0 ||
           std::memcmp(chunks[i].tag.data(),"SHDR",4u)==0) {
            if(code_index!=static_cast<std::size_t>(-1)) return false;
            code_index=i;
        }
    if(code_index==static_cast<std::size_t>(-1)) return false;

    try {
        auto &payload=chunks[code_index].payload;
        payload.resize(words.size()*4u);
        for(std::size_t i=0u;i<words.size();++i)
            write_u32(payload.data()+i*4u,words[i]);

        const auto header_size=32u+4u*chunks.size();
        if(basis_size<32u || header_size>
            std::numeric_limits<std::uint32_t>::max())
            return false;

        out.assign(basis,basis+std::min<std::size_t>(basis_size,32u));
        out.resize(header_size,0u);
        std::memcpy(out.data(),basis,24u);
        write_u32(out.data()+28u,static_cast<std::uint32_t>(chunks.size()));

        std::vector<std::uint32_t> offsets;
        offsets.reserve(chunks.size());
        for(const auto &c:chunks) {
            if(out.size()>std::numeric_limits<std::uint32_t>::max())
                return false;
            offsets.push_back(static_cast<std::uint32_t>(out.size()));
            out.insert(out.end(),
                reinterpret_cast<const std::uint8_t *>(c.tag.data()),
                reinterpret_cast<const std::uint8_t *>(c.tag.data())+4u);
            const auto size_at=out.size();
            out.resize(size_at+4u);
            write_u32(out.data()+size_at,
                static_cast<std::uint32_t>(c.payload.size()));
            out.insert(out.end(),c.payload.begin(),c.payload.end());
        }

        if(out.size()>std::numeric_limits<std::uint32_t>::max())
            return false;
        write_u32(out.data()+24u,static_cast<std::uint32_t>(out.size()));
        for(std::size_t i=0u;i<offsets.size();++i)
            write_u32(out.data()+32u+i*4u,offsets[i]);
        std::fill(out.begin()+4u,out.begin()+20u,std::uint8_t{0});
        return legacy_plan::dxbc::fix_checksum(out.data(),out.size());
    } catch(...) {
        out.clear();
        return false;
    }
}

bool apply_insertions(
    std::vector<std::uint32_t> &words,
    std::vector<insertion> insertions) noexcept
{
    try {
        std::stable_sort(
            insertions.begin(),insertions.end(),
            [](const insertion &a,const insertion &b){
                return a.word>b.word;
            });
        for(const auto &ins:insertions) {
            if(ins.word>words.size()) return false;
            words.insert(
                words.begin()+static_cast<std::ptrdiff_t>(ins.word),
                ins.payload.begin(),ins.payload.end());
        }
        words[1]=static_cast<std::uint32_t>(words.size());
        return true;
    } catch(...) {
        return false;
    }
}

} // namespace

fixed_local_single_materialize_outcome
materialize_fixed_local_specular_single(
    const core::feature_registry &features,
    const std::uint8_t *source,
    std::size_t size,
    std::vector<std::uint8_t> &output) noexcept
{
    fixed_local_single_materialize_outcome out;
    output.clear();

    const auto samples=
        locate_fixed_local_specular_material_samples(source,size);
    if(samples.result==
        fixed_local_specular_material_sample_result::
            pass_not_fixed_local_specular)
        return out;
    if(samples.result!=
        fixed_local_specular_material_sample_result::exact) {
        out.result=fixed_local_single_materialize_result::fail_contract;
        return out;
    }
    if(samples.topology==
        fixed_local_specular_material_topology::blended_diffuse_spec) {
        out.result=
            fixed_local_single_materialize_result::
                pass_blended_requires_endpoint_b;
        return out;
    }
    if(samples.topology!=
        fixed_local_specular_material_topology::single_diffuse_spec) {
        out.result=fixed_local_single_materialize_result::fail_contract;
        return out;
    }

    const auto geometry=attest_fixed_local_geometry_contract(source,size);
    if(geometry.result!=fixed_local_geometry_result::exact) {
        out.result=fixed_local_single_materialize_result::fail_geometry_capture;
        return out;
    }

    std::vector<std::uint8_t> p22;
    const auto a1=legacy_plan::materialize_a1_create_time(
        features,source,size,p22);
    if(a1.result!=legacy_plan::a1_create_time_result::applied ||
       !a1.full_plan_materialized) {
        out.result=
            fixed_local_single_materialize_result::
                pass_a1_not_fully_materialized;
        return out;
    }
    out.a1_full_plan=true;

    std::vector<chunk> chunks;
    std::vector<std::uint32_t> words;
    std::size_t code_index=0u;
    if(!parse(p22.data(),p22.size(),chunks,code_index,words)) {
        out.result=fixed_local_single_materialize_result::fail_invalid_dxbc;
        return out;
    }

    std::vector<instruction> instructions;
    if(!decode(words,instructions)) {
        out.result=fixed_local_single_materialize_result::fail_invalid_dxbc;
        return out;
    }

    std::uint32_t color0=0u;
    if(!parse_color0(chunks,color0)) {
        out.result=fixed_local_single_materialize_result::fail_color0_signature;
        return out;
    }
    out.color0_input_register=color0;

    std::uint32_t t1_decl=0u,temps_word=0u,temp_count=0u;
    if(!locate_t1_decl_and_temps(
            words,instructions,t1_decl,temps_word,temp_count)) {
        out.result=fixed_local_single_materialize_result::fail_declaration_shape;
        return out;
    }
    out.original_temp_count=temp_count;

    constexpr std::uint32_t k_scratch_count=6u;
    const auto spec_mat=temp_count+0u;
    const auto diff_mat=temp_count+1u;
    const auto accum=temp_count+2u;
    const auto q=temp_count+3u;
    const auto work=temp_count+4u;
    const auto distance=temp_count+5u;
    if(distance>4095u) {
        out.result=fixed_local_single_materialize_result::fail_declaration_shape;
        return out;
    }
    out.final_temp_count=temp_count+k_scratch_count;

    const auto *t1_sample=
        find_instruction(instructions,samples.specular_a_t1.instruction_word);
    const instruction *c156_add=nullptr;
    if(t1_sample==nullptr || t1_sample->end-t1_sample->start!=11u ||
       !locate_c156_add(
            words,instructions,samples.diffuse_a_t0,c156_add)) {
        out.result=fixed_local_single_materialize_result::fail_material_capture;
        return out;
    }

    // Patch original coordinates before any structural insertion.
    words[temps_word+1u]=out.final_temp_count;
    const auto &cut=samples.island.output_cut;
    if(cut.local_operand_index_word>=words.size() ||
       words[cut.local_operand_token_word]!=0x00100246u) {
        out.result=fixed_local_single_materialize_result::fail_contract;
        return out;
    }
    words[cut.local_operand_index_word]=accum;
    out.output_cut_redirected=true;

    std::vector<insertion> insertions;

    // New declarations are inserted immediately before dcl_temps.
    insertion declarations{};
    declarations.word=temps_word;
    append(declarations.payload,k_b12_decl);

    std::array<std::uint32_t,4> t10_decl_words{};
    std::copy_n(
        words.begin()+static_cast<std::ptrdiff_t>(t1_decl),
        4u,t10_decl_words.begin());
    t10_decl_words[2]=10u;
    append(declarations.payload,t10_decl_words);

    fixed_local_specular_t19_decl t19_decl{};
    if(emit_fixed_local_specular_t19_decl(t19_decl)!=
        fixed_local_specular_t19_emit_result::exact) {
        out.result=fixed_local_single_materialize_result::fail_island_emit;
        return out;
    }
    append(declarations.payload,t19_decl.words);
    insertions.push_back(std::move(declarations));
    out.b12_declared=true;
    out.t10_declared=true;
    out.t19_declared=true;

    // Clone stock t1 sampling to exact PTDE sidecar t10 and immediately
    // materialize Mspec0 = SpecRGB * raw_c101 * COLOR0.
    insertion spec_capture{};
    spec_capture.word=t1_sample->end;
    spec_capture.payload.assign(
        words.begin()+t1_sample->start,
        words.begin()+t1_sample->end);
    if(spec_capture.payload[3]<0x10u ||
       spec_capture.payload[8]!=1u) {
        out.result=fixed_local_single_materialize_result::fail_material_capture;
        return out;
    }
    spec_capture.payload[3]-=0x10u; // preserve stock alpha, write RGB to .yzw
    spec_capture.payload[8]=10u;
    append_mul_vec_temp_cb(
        spec_capture.payload,
        spec_mat,
        k_temp_src_yzw,
        samples.specular_a_t1.destination_register,
        2u);
    append_mul_vec_temp_input(
        spec_capture.payload,spec_mat,spec_mat,color0);
    insertions.push_back(std::move(spec_capture));

    // t0 + c156 is the audited pre-material diffuse seam.
    insertion diffuse_capture{};
    diffuse_capture.word=c156_add->end;
    append_mul_vec_temp_cb(
        diffuse_capture.payload,
        diff_mat,
        k_temp_src_xyz,
        words[c156_add->start+2u],
        1u);
    append_mul_vec_temp_input(
        diffuse_capture.payload,diff_mat,diff_mat,color0);
    append_mov_zero_xyz(diffuse_capture.payload,accum);
    insertions.push_back(std::move(diffuse_capture));

    // Each fixed light reuses stock geometry N/V/L but owns raw q, linear
    // attenuation and the complete PTDE legacy local specular branch.
    for(std::uint8_t light=0u;
        light<samples.island.light_count;
        ++light) {
        const auto &geo=geometry.lights[light];
        const auto *sqrt_ins=
            find_instruction(instructions,geo.distance_sqrt_word);
        if(sqrt_ins==nullptr ||
           sqrt_ins->end-sqrt_ins->start!=5u) {
            out.result=
                fixed_local_single_materialize_result::fail_geometry_capture;
            return out;
        }
        const auto sqrt_component=
            scalar_component(words[sqrt_ins->start+1u]);
        if(sqrt_component>3u) {
            out.result=
                fixed_local_single_materialize_result::fail_geometry_capture;
            return out;
        }

        insertion distance_capture{};
        distance_capture.word=sqrt_ins->end;
        append_mov_scalar(
            distance_capture.payload,
            distance,
            temp_src_scalar(sqrt_component),
            words[sqrt_ins->start+2u]);
        insertions.push_back(std::move(distance_capture));

        const auto &window=
            samples.island.output_cut.operands.plan.lights[light].
                microfacet_window;
        std::array<instruction,4> linear{};
        if(!locate_linear_template(
                words,instructions,
                window.start_word,
                window.end_word_exclusive,
                112u+light,
                116u+light,
                linear)) {
            out.result=
                fixed_local_single_materialize_result::fail_geometry_capture;
            return out;
        }

        std::vector<std::uint32_t> linear_words;
        if(!retarget_linear_template(
                words,linear,work,distance,linear_words)) {
            out.result=
                fixed_local_single_materialize_result::fail_geometry_capture;
            return out;
        }

        insertion island{};
        if(window.end_word_exclusive==0u) {
            out.result=fixed_local_single_materialize_result::fail_contract;
            return out;
        }
        island.word=window.end_word_exclusive-1u; // immediately before ENDIF
        append(island.payload,linear_words.data(),linear_words.size());

        fixed_local_specular_t19_load q_load{};
        if(emit_fixed_local_specular_t19_load(
                q,light,q_load)!=
            fixed_local_specular_t19_emit_result::exact) {
            out.result=fixed_local_single_materialize_result::fail_island_emit;
            return out;
        }
        append(island.payload,q_load.words);

        const auto &kernel=samples.island.angular[light];
        append(island.payload,kernel.words);

        const auto &operands=
            samples.island.output_cut.operands.lights[light];
        const auto ndotl_component=
            scalar_component(operands.ndotl_scalar_dst.token[0]);
        if(ndotl_component>3u) {
            out.result=fixed_local_single_materialize_result::fail_island_emit;
            return out;
        }
        append_max_scalar_zero(
            island.payload,operands.ndotl_scalar_dst);

        // q *= attenuation
        append_mul_vec_scalar(
            island.payload,q,q,work,0u);

        // diffuse: accum += q * NdotL * Mdiff0
        append_mul_vec_scalar(
            island.payload,work,q,
            operands.ndotl_scalar_dst.token[1],
            ndotl_component);
        append_mad_accum(
            island.payload,accum,work,diff_mat);

        // specular: accum += q * angular * Mspec0
        append_mul_vec_scalar(
            island.payload,work,q,
            kernel.result_temp_register,
            kernel.result_component);
        append_mad_accum(
            island.payload,accum,work,spec_mat);

        insertions.push_back(std::move(island));
    }

    insertion common_c139{};
    common_c139.word=cut.join_word;
    append_mul_accum_c139(common_c139.payload,accum);
    insertions.push_back(std::move(common_c139));

    if(!apply_insertions(words,std::move(insertions))) {
        out.result=fixed_local_single_materialize_result::fail_rebuild;
        return out;
    }

    if(!strip_rdef(chunks,out.rdef_stripped) ||
       !rebuild(
            p22.data(),p22.size(),
            std::move(chunks),code_index,
            words,output)) {
        output.clear();
        out.result=fixed_local_single_materialize_result::fail_rebuild;
        return out;
    }

    if(!legacy_plan::dxbc::checksum_container_valid(
            output.data(),output.size())) {
        output.clear();
        out.result=fixed_local_single_materialize_result::fail_postcondition;
        return out;
    }

    out.light_count=samples.island.light_count;
    out.result=fixed_local_single_materialize_result::applied;
    return out;
}

} // namespace dsrrl::operators::point_light
