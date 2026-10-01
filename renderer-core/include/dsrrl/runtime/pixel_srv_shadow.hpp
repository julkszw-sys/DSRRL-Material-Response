#pragma once

#include <array>
#include <cstdint>

#include <reshade.hpp>

struct ID3D11ShaderResourceView;
struct ID3D11SamplerState;

namespace dsrrl::runtime {

constexpr std::uint32_t k_pixel_srv_shadow_slots = 16u;
constexpr std::uint32_t k_pixel_sampler_shadow_slots = 16u;

void pixel_srv_shadow_reset() noexcept;

void pixel_srv_shadow_on_push_descriptors(
    reshade::api::command_list *cmd_list,
    reshade::api::shader_stage stages,
    reshade::api::pipeline_layout layout,
    std::uint32_t param_index,
    const reshade::api::descriptor_table_update &update) noexcept;

bool pixel_srv_shadow_snapshot(
    reshade::api::command_list *cmd_list,
    std::uint32_t first,
    std::uint32_t count,
    ID3D11ShaderResourceView **out) noexcept;

bool pixel_sampler_shadow_snapshot(
    reshade::api::command_list *cmd_list,
    std::uint32_t first,
    std::uint32_t count,
    ID3D11SamplerState **out) noexcept;

} // namespace dsrrl::runtime
