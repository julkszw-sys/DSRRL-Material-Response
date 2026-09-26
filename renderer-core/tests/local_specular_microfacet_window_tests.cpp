#include "dsrrl/operators/point_light/local_specular_microfacet_windows.hpp"

#include <cstdint>
#include <iostream>
#include <vector>

using namespace dsrrl::operators::point_light;

namespace {

bool check(bool condition,const char *expr,int line)
{
    if(condition) return true;
    std::cerr<<"CHECK FAILED line "<<line<<": "<<expr<<'\n';
    return false;
}
#define CHECK(e) do { if(!check(static_cast<bool>(e),#e,__LINE__)) return 1; } while(false)

constexpr std::uint32_t op(
    std::uint32_t opcode,
    std::uint32_t length=1u)
{
    return opcode | (length << 24u);
}

void append_window(
    std::vector<std::uint32_t> &words,
    std::uint32_t instruction_count,
    bool valid_sequence=true)
{
    constexpr std::uint32_t k_if=31u;
    constexpr std::uint32_t k_endif=21u;
    constexpr std::uint32_t k_dp3=16u;
    constexpr std::uint32_t k_mad=50u;
    constexpr std::uint32_t k_mul=56u;
    constexpr std::uint32_t k_exp=25u;
    constexpr std::uint32_t k_nop=57u;
    constexpr std::uint32_t k_a=0xc0b1c059u;
    constexpr std::uint32_t k_b=0xc0df760cu;

    words.push_back(op(k_if));

    const auto fixed_instructions=8u;
    for(std::uint32_t i=0u;
        i<instruction_count-fixed_instructions;
        ++i)
        words.push_back(op(k_nop));

    words.push_back(op(k_dp3));
    words.push_back(op(k_dp3));
    words.push_back(op(valid_sequence ? k_dp3 : k_nop));
    words.push_back(op(k_mad,3u));
    words.push_back(k_a);
    words.push_back(k_b);
    words.push_back(op(k_mul));
    words.push_back(op(k_exp));
    words.push_back(op(k_endif));
}

std::vector<std::uint32_t> stream(
    std::uint32_t lights,
    std::uint32_t window_instructions,
    bool valid_sequence=true)
{
    std::vector<std::uint32_t> words{0u,0u};
    for(std::uint32_t i=0u;i<lights;++i)
        append_window(
            words,
            window_instructions,
            valid_sequence);
    words[1]=static_cast<std::uint32_t>(words.size());
    return words;
}

} // namespace

int main()
{
    {
        const auto words=stream(1u,34u);
        const auto scan=
            scan_local_specular_microfacet_shex_words(
                words.data(),
                words.size(),
                local_specular_receiver_class::
                    clustered_spc_pnts);
        CHECK(scan.result==
            local_specular_window_result::exact);
        CHECK(scan.window_count==1u);
        CHECK(scan.windows[0].light_ordinal==0u);
        CHECK(scan.windows[0].instruction_count==34u);
    }

    {
        const auto words=stream(2u,60u);
        const auto scan=
            scan_local_specular_microfacet_shex_words(
                words.data(),
                words.size(),
                local_specular_receiver_class::
                    fixed_spc_pntss);
        CHECK(scan.result==
            local_specular_window_result::exact);
        CHECK(scan.window_count==2u);
        CHECK(scan.windows[1].light_ordinal==1u);
        CHECK(scan.windows[0].instruction_count==60u);
        CHECK(scan.windows[1].instruction_count==60u);
    }

    {
        const auto words=stream(4u,63u);
        const auto scan=
            scan_local_specular_microfacet_shex_words(
                words.data(),
                words.size(),
                local_specular_receiver_class::
                    fixed_spc_pntssss);
        CHECK(scan.result==
            local_specular_window_result::exact);
        CHECK(scan.window_count==4u);
        CHECK(scan.windows[3].light_ordinal==3u);
    }

    {
        const auto words=stream(2u,60u);
        const auto scan=
            scan_local_specular_microfacet_shex_words(
                words.data(),
                words.size(),
                local_specular_receiver_class::
                    fixed_spc_pntssss);
        CHECK(scan.result==
            local_specular_window_result::
                fail_open_anchor_count);
    }

    {
        const auto words=stream(1u,34u,false);
        const auto scan=
            scan_local_specular_microfacet_shex_words(
                words.data(),
                words.size(),
                local_specular_receiver_class::
                    clustered_spc_pnts);
        CHECK(scan.result==
            local_specular_window_result::
                fail_open_anchor_shape);
    }

    {
        auto words=stream(1u,34u);
        words[2]=op(31u,127u);
        const auto scan=
            scan_local_specular_microfacet_shex_words(
                words.data(),
                words.size(),
                local_specular_receiver_class::
                    clustered_spc_pnts);
        CHECK(scan.result==
            local_specular_window_result::
                fail_open_instruction_stream);
    }

    std::cout
        <<"local_specular_microfacet_window_tests: PASS\n";
    return 0;
}
