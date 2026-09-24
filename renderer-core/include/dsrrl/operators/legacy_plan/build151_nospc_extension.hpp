#pragma once

#include "dsrrl/core/feature_registry.hpp"
#include "dsrrl/operators/legacy_plan/dxbc_checksum.hpp"
#include "dsrrl/operators/legacy_plan/sha256_bytes.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string_view>
#include <vector>

namespace dsrrl::operators::legacy_plan::build151 {

struct nospc_plan {
    const char *representative;
    const char *original_sha256;
    std::uint32_t code_size;
    std::uint32_t byte_offset;
    const char *replacement_sha256;
};

inline constexpr std::array<nospc_plan, 24> k_missing24_nospc = {{
    {"FRPG_Phn_Dif___Bmp___LitCsd_HemEnv.fpo","01299fee144ee1ef2aa63c0f5633279a155e37c2c4bd6dbb546967cf7890b04f",19712u,14664u,"2581d7def37b07c2de73ab3ded53ebf849fc87fc7f58fab2597a8744367a724e"},
    {"FRPG_Phn_Dif___Bmp___LitCsd_HemEnvLerp.fpo","a88f80cacd20e16d139af1d25b3dd5e000f01e065ef8bef79e821e496136a000",20236u,15052u,"2b1735b88a8ca92c61b1e6c0d9968f5b763e5f73a55c7705f3ddb3d2851aaeb3"},
    {"FRPG_Phn_Dif___Bmp___LitSdw_HemEnv.fpo","0fa0778ed073c62de9c491785088ee376b1a124980a356d0181509e8b57b8d7e",19412u,14324u,"bed52c06146fbfee7aa4d92aa13d9138ace05dabe13d3f2b383a1260c896709b"},
    {"FRPG_Phn_Dif___Bmp___LitSdw_HemEnvLerp.fpo","a918de8a24760d4d4dcd0a32e8b9b3878734f1de4fe05e41fd64c02bee12111f",19936u,14712u,"df40dceac988e38520d3d497b14dd71f87fa858bf2c13b0b9b8c165388c69882"},
    {"FRPG_Phn_Dif___Bmp___Lit____HemEnv.fpo","f0ce5a55ebec00d41ce814fc9028457400f25bf2463c4a486bdcd50265468a0c",17928u,12840u,"efcdf5b20ed15c365c31f18972688a148b2b74a4efbc19e5a2e4d458a85b53bf"},
    {"FRPG_Phn_Dif___Bmp___Lit____HemEnvLerp.fpo","02ced4a14434029b8dcb9989a6ceae6c018982e1ac77d2afbbfdebbe05662fe8",18448u,13224u,"1f3ac00a70ec509fd3560ff16724da65ab42cb81369f39d340a9b1a85e87051e"},
    {"FRPG_Phn_Dif___Bmp______Csd_HemEnv.fpo","6af3e47d98146b5ea66887c280b560460266ee0eb4768e7b739a1603f4909993",19244u,14344u,"95b61372dd9bc0201a3dac982bf93f28eca8647f65703d167deaad6b3b849dab"},
    {"FRPG_Phn_Dif___Bmp______Csd_HemEnvLerp.fpo","1046e110fd96fa0dc19f885fddce91c4010598ad33202c6b0a8cb594f3618e30",19764u,14728u,"967d32ec58ecf82b82aeb23a463fbf6c5b98eaaf7dd4ac3d2cdf8073f836fdda"},
    {"FRPG_Phn_Dif___Bmp______Sdw_HemEnv.fpo","c9333926f06e5a65cbc5950eb8d14d43cb9a80ebc2233d530b6af79d96c84e30",18944u,14004u,"dbab3573623714195d59cbcaf6bd2d0e3c0199391cdb405bbb7016f2e946f29c"},
    {"FRPG_Phn_Dif___Bmp______Sdw_HemEnvLerp.fpo","e4bdb1f29660c0c1e5748368c3e25da7165687e366e91fe0dd48a041c1346fea",19464u,14388u,"ec6ea66975625916029cc83477d4ece519785b6857161e7962896466a78a60ab"},
    {"FRPG_Phn_Dif___Bmp__________HemEnv.fpo","a3bf7855d74dd579acadbdcb2feba602972b50e7c3d829416e7b1e41766ca301",17448u,12536u,"e135a31111ed3d820e0f5a6a5df8d55f4374b5c165a98934694ee285334b1ac2"},
    {"FRPG_Phn_Dif___Bmp__________HemEnvLerp.fpo","8382a4492342ae76bec3432baffe37bac1e179af343d67b8a71fd0dcb6e7048d",17968u,12920u,"2b63b7e3a077dd88f8c054bd30cb1178e118c6d134b4d3ea9f161e85536ac59a"},
    {"FRPG_Phn_Dif_________LitCsd_HemEnv.fpo","362646161ca19e2005f786cd17ce8a851ea27575531ffbe2d362c1af2036d443",18552u,13504u,"aaf7df13cc518879deea149859b31278d0c81b09d08b4875766e6db383f87d8e"},
    {"FRPG_Phn_Dif_________LitCsd_HemEnvLerp.fpo","344c22433cc457b26d37f882bb400f8ab83d1c7c2d6de2861360a7f19a6ae1b2",19072u,13888u,"0d343a17e5a7554a6c513a84c820b272fe5f2174ce7c1d92575023bcd3ba1a14"},
    {"FRPG_Phn_Dif_________LitSdw_HemEnv.fpo","576797104b5e186e728175f73fff36ef06fc9b69cc4a5d78fb092077add901ea",18252u,13164u,"ab6ab667be0e4251a8e3b801a3640f11db2d46afca8459c13d999c93d1ad5cd2"},
    {"FRPG_Phn_Dif_________LitSdw_HemEnvLerp.fpo","1124469ba634d73790a19f26d022236e8f7565c823ae45a6ca9eb935ce05d7b5",18772u,13548u,"89a83c2b0303a96007e28325b3725434e5cfd05b1d4de3ab928528f4cadfd566"},
    {"FRPG_Phn_Dif_________Lit____HemEnv.fpo","b8fdbd18f7c283049514961f3541d12f42f851ef9d51833e35c64c1a3c88cd2b",16768u,11680u,"98ff950df3f0d77aabb7872b1fc3622505130cb10043573e6f96629dd9bcf9c3"},
    {"FRPG_Phn_Dif_________Lit____HemEnvLerp.fpo","d1054cc56a875c79cddf7459827e18bb16c0557a04c34aac2cb5e457ed30e231",17288u,12064u,"f00e7ed36ad952443bcd60841714ce84e659f853adec5508a7ba5f06089fd736"},
    {"FRPG_Phn_Dif____________Csd_HemEnv.fpo","e4fe85eb47aad8af4773dd66128c52d299d7c0f1318de0306de4e6170e5d12d2",18084u,13184u,"d2d985016bf6ad576c015c856c68d879546d875caf855f77b7b4c159e2c93c25"},
    {"FRPG_Phn_Dif____________Csd_HemEnvLerp.fpo","faa8b38126bf28e653c593abed0774dc72726564f6c95fb202fd2636d24840b8",18604u,13568u,"025acb7db67d49d90e20a62ebd278d9005f67e63510dc615c60dab43629d4ed9"},
    {"FRPG_Phn_Dif____________Sdw_HemEnv.fpo","69351fae66925f04b0abe071c046360c8a032a78e9c21d5c8b9a5b34d4991d40",17784u,12844u,"21fde1ee60fb6ebd2d05dbcfe6affb54193228ab66435eedfcbc5731159ece4b"},
    {"FRPG_Phn_Dif____________Sdw_HemEnvLerp.fpo","a80b26b1cb59d62a61b14b16aed45136b663f4d312ee015db657141167cfe2f9",18304u,13228u,"57b1c03d815ff81d0841edd83f0f09e26dbcf2f1749ef7ce3c2d544ef606fa35"},
    {"FRPG_Phn_Dif________________HemEnv.fpo","43aeadb253ae63c4bf5e5fed36a037630a55798bf8cb78494879fe0ec0fee7dd",16284u,11372u,"28f696e8c0b991d2f78d8c62631efb3f90460dc3e8adbbd3107363096cc4bf87"},
    {"FRPG_Phn_Dif________________HemEnvLerp.fpo","0d6d431986318f95e2f0ce376540062aa98c963bf4fe0a308cf72ed6fd8022c9",16808u,11760u,"b3594b15a58f66d05c1a0d0d5ac6108cd5ec90377c95208bd3703228f7e32729"},
}};

inline constexpr std::uint16_t k_plan_index_base = 144u;
inline constexpr std::uint32_t k_expected_old_word = 0x07000038u;
inline constexpr std::uint32_t k_replacement_word = 0x07000031u;

enum class nospc_result : std::uint8_t {
    applied = 0,
    pass_through_not_candidate_size,
    pass_through_unknown_exact_sha,
    pass_through_no_enabled_owner,
    fail_open_invalid_dxbc,
    fail_open_offset,
    fail_open_token_mismatch,
    fail_open_checksum,
    fail_open_output_sha
};

struct nospc_outcome {
    nospc_result result = nospc_result::pass_through_unknown_exact_sha;
    std::uint16_t plan_index = 0xffffu;
    hashing::sha256_digest source_sha256{};
    hashing::sha256_digest output_sha256{};
};

inline bool candidate_code_size(std::size_t size) noexcept
{
    for (const auto &p : k_missing24_nospc)
        if (p.code_size == size)
            return true;
    return false;
}

inline const nospc_plan *find_plan(
    std::size_t size,
    const hashing::sha256_digest &digest,
    std::uint16_t &plan_index) noexcept
{
    for (std::size_t i = 0; i < k_missing24_nospc.size(); ++i) {
        const auto &p = k_missing24_nospc[i];
        if (p.code_size != size)
            continue;
        if (!hashing::matches_hex(digest, p.original_sha256))
            continue;
        plan_index = static_cast<std::uint16_t>(
            k_plan_index_base + i);
        return &p;
    }
    return nullptr;
}

inline nospc_outcome materialize(
    const core::feature_registry &features,
    const std::uint8_t *source,
    std::size_t size,
    std::vector<std::uint8_t> &output) noexcept
{
    nospc_outcome out;
    output.clear();

    if (!candidate_code_size(size)) {
        out.result = nospc_result::pass_through_not_candidate_size;
        return out;
    }

    if (!dxbc::checksum_container_valid(source, size)) {
        out.result = nospc_result::fail_open_invalid_dxbc;
        return out;
    }

    out.source_sha256 = hashing::sha256(source, size);

    std::uint16_t plan_index = 0xffffu;
    const auto *plan = find_plan(
        size,
        out.source_sha256,
        plan_index);

    if (plan == nullptr) {
        out.result = nospc_result::pass_through_unknown_exact_sha;
        return out;
    }

    out.plan_index = plan_index;

    if (!features.enabled(core::operator_id::envspec_nospc_delete)) {
        out.result = nospc_result::pass_through_no_enabled_owner;
        return out;
    }

    if (plan->byte_offset > size ||
        size - plan->byte_offset < sizeof(std::uint32_t)) {
        out.result = nospc_result::fail_open_offset;
        return out;
    }

    std::uint32_t word = 0;
    std::memcpy(
        &word,
        source + plan->byte_offset,
        sizeof(word));

    if (word != k_expected_old_word) {
        out.result = nospc_result::fail_open_token_mismatch;
        return out;
    }

    try {
        output.assign(source, source + size);
    } catch (...) {
        output.clear();
        out.result = nospc_result::fail_open_offset;
        return out;
    }

    std::memcpy(
        output.data() + plan->byte_offset,
        &k_replacement_word,
        sizeof(k_replacement_word));

    if (!dxbc::fix_checksum(output.data(), output.size())) {
        output.clear();
        out.result = nospc_result::fail_open_checksum;
        return out;
    }

    out.output_sha256 =
        hashing::sha256(output.data(), output.size());

    if (!hashing::matches_hex(
            out.output_sha256,
            plan->replacement_sha256)) {
        output.clear();
        out.result = nospc_result::fail_open_output_sha;
        return out;
    }

    out.result = nospc_result::applied;
    return out;
}

} // namespace dsrrl::operators::legacy_plan::build151
