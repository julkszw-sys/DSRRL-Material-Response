#include "dsrrl/operators/point_light/fixed_local_specular_t19_lowering.hpp"

namespace dsrrl::operators::point_light {

fixed_local_specular_t19_emit_result
emit_fixed_local_specular_t19_decl(
    fixed_local_specular_t19_decl &out) noexcept
{
    out.words = {{
        0x040000a2u,
        0x00107000u,
        19u,
        16u
    }};
    return fixed_local_specular_t19_emit_result::exact;
}

fixed_local_specular_t19_emit_result
emit_fixed_local_specular_t19_load(
    std::uint32_t destination_temp,
    std::uint8_t light_ordinal,
    fixed_local_specular_t19_load &out) noexcept
{
    out = {};
    if (destination_temp > 4095u)
        return fixed_local_specular_t19_emit_result::fail_invalid_register;
    if (light_ordinal > 3u)
        return fixed_local_specular_t19_emit_result::fail_invalid_light_ordinal;

    out.words = {{
        0x8b0000a7u,
        0x80018302u,
        0x00199983u,
        0x001000f2u,
        destination_temp,
        0x00004001u,
        static_cast<std::uint32_t>(light_ordinal),
        0x00004001u,
        0u,
        0x00107e46u,
        19u
    }};
    return fixed_local_specular_t19_emit_result::exact;
}

} // namespace dsrrl::operators::point_light
