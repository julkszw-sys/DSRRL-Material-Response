#include "dsrrl/runtime/island_draw_adapter.hpp"

#include <cstdint>
#include <iostream>

#define CHECK(x) do { if(!(x)){ std::cerr << "CHECK failed: " #x "\n"; return 1; } } while(false)

int main()
{
    using namespace dsrrl;

    const auto point =
        core::operator_bit(core::operator_id::point_light);
    const auto local =
        core::operator_bit(core::operator_id::local_specular_legacy);
    const auto mr =
        core::operator_bit(core::operator_id::material_response);

    auto *ps =
        reinterpret_cast<ID3D11PixelShader *>(std::uintptr_t{1});
    auto *cb =
        reinterpret_cast<ID3D11Buffer *>(std::uintptr_t{2});
    auto *t18 =
        reinterpret_cast<ID3D11ShaderResourceView *>(std::uintptr_t{3});
    auto *t19 =
        reinterpret_cast<ID3D11ShaderResourceView *>(std::uintptr_t{4});

    {
        runtime::island_draw_adapter_request request{};
        request.primary = core::operator_id::local_specular_legacy;
        request.additional_owners = point | mr;
        request.additional_shader_owners = point | mr;
        request.additional_constant_buffer_owners = point | mr;
        request.additional_resource_owners = point;
        request.receiver_verified = true;
        request.material_verified = true;
        request.pixel_shader = ps;
        request.replace_pixel_shader = true;
        request.constant_buffers[0] = {12u, cb, local | point | mr};
        request.constant_buffer_count = 1u;
        request.srvs[0] = {18u, t18};
        request.srvs[1] = {19u, t19};
        request.srv_count = 2u;

        runtime::draw_tx_mutation mutation{};
        CHECK(runtime::build_island_draw_mutation(
                  request, mutation) ==
              runtime::island_draw_adapter_result::ready);
        CHECK(mutation.owners == (local | point | mr));
        CHECK(mutation.shader_owners == (local | point | mr));
        CHECK(mutation.constant_buffer_owners == (local | point | mr));
        CHECK(mutation.resource_owners == (local | point));
        CHECK(mutation.constant_buffer_count == 1u);
        CHECK(mutation.srv_count == 2u);
    }

    {
        runtime::island_draw_adapter_request request{};
        request.primary = core::operator_id::point_light;
        request.additional_owners = mr;
        request.additional_shader_owners = mr;
        request.additional_constant_buffer_owners = mr;
        request.additional_resource_owners = 0u;
        request.receiver_verified = true;
        request.material_verified = true;
        request.pixel_shader = ps;
        request.replace_pixel_shader = true;
        request.constant_buffers[0] = {12u, cb, point | mr};
        request.constant_buffer_count = 1u;
        request.srvs[0] = {18u, t18};
        request.srvs[1] = {19u, t19};
        request.srv_count = 2u;

        runtime::draw_tx_mutation mutation{};
        CHECK(runtime::build_island_draw_mutation(
                  request, mutation) ==
              runtime::island_draw_adapter_result::ready);
        CHECK(mutation.owners == (point | mr));
        CHECK(mutation.shader_owners == (point | mr));
        CHECK(mutation.constant_buffer_owners == (point | mr));
        CHECK(mutation.resource_owners == point);
        CHECK(mutation.constant_buffer_count == 1u);
        CHECK(mutation.srv_count == 2u);
    }

    std::cout << "clustered_pnts_draw_adapter_tests: PASS\n";
    return 0;
}
