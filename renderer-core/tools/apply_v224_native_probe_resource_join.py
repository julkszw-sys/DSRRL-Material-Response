#!/usr/bin/env python3
"""V224 opt-in verified native t12 resource identity recovery, never a guessed probe.

Materialize after V221/V222: the V223 capture proved a live t12 SRV and
an exact nonmetal material, but native-probe view registry missed. A second SRV
for an already SHA256-authenticated BC6H cube can be joined by its underlying
ID3D11Resource identity, without assigning the P_Metal probe arbitrarily.

Only a full cube view of a matching, live hash-registered resource can produce
a PTDE PackedGI carrier. Unknown resources remain stock DSR. P_Metal untouched.
"""
import argparse
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
HEADER = ROOT / "renderer-core/include/dsrrl/runtime/envspec_resource_runtime.hpp"
RESOURCE = ROOT / "renderer-core/src/runtime/envspec_resource_runtime.cpp"
DRAW = ROOT / "renderer-core/src/runtime/pmetal_envspec_draw_runtime.cpp"

HEADER_ANCHOR = "    // Full-PTDE HemEnv resource cut."
RESOURCE_ANCHOR = "bool envspec_resource_runtime::prepare_envdiffuse("
DRAW_ANCHOR = "#if defined(DSRRL_V222_EXACT_PROBE_REJECT_FRONTIER)"

HEADER_ADD = """#if defined(DSRRL_V224_EXACT_NONMETAL_T12_RESOURCE_JOIN)
    // Read-only identity witness even when the live SRV has no view-registry
    // entry. A successful join requires the *underlying* resource to already
    // exist in the SHA256-authenticated native BC6H texture registry.
    struct native_resource_endpoint_v224 {
        std::uint64_t resource_key = 0u;
        std::uint32_t srv_dimension = 0u;
        std::uint32_t srv_format = 0u;
        std::uint32_t resource_format = 0u;
        std::uint32_t width = 0u;
        std::uint32_t height = 0u;
        std::uint32_t mip_levels = 0u;
        std::uint32_t array_size = 0u;
        std::uint16_t probe = 0u;
        bool view_registered = false;
        bool resource_registered = false;
        bool view_shape_valid = false;
        bool exact_native = false;
    };
    struct native_resource_join_v224 {
        native_resource_endpoint_v224 a{};
        native_resource_endpoint_v224 b{};
        bool pack_ready = false;
        bool sampler_ready = false;
        bool prepared = false;
    };
    bool prepare_exact_native_resource_join_v224(
        ID3D11DeviceContext *context,
        std::uint8_t slot,
        bool probe_b_required,
        prepared_envspec_resources &prepared,
        native_resource_join_v224 &diag) noexcept;
#endif

"""

RESOURCE_ADD = """#if defined(DSRRL_V224_EXACT_NONMETAL_T12_RESOURCE_JOIN)
bool envspec_resource_runtime::prepare_exact_native_resource_join_v224(
    ID3D11DeviceContext *context,
    std::uint8_t slot,
    bool probe_b_required,
    prepared_envspec_resources &prepared,
    native_resource_join_v224 &diag) noexcept
{
    prepared = {};
    diag = {};
    if (context == nullptr || slot >= k_ptde_slots)
        return false;

    // The draw's exact receiver/material gate is enforced by its caller.
    // Here we authenticate only the native cubemap's RESOURCE identity:
    // no native SHA registration -> no guessed PackedGI probe.
    ID3D11ShaderResourceView *views[3]{};
    context->PSGetShaderResources(
        12u, probe_b_required ? 3u : 1u, views);

    const auto verify = [](
        ID3D11ShaderResourceView *view,
        native_resource_endpoint_v224 &observed) noexcept -> bool {
        if (view == nullptr)
            return false;

        D3D11_SHADER_RESOURCE_VIEW_DESC srv{};
        view->GetDesc(&srv);
        observed.srv_dimension =
            static_cast<std::uint32_t>(srv.ViewDimension);
        observed.srv_format =
            static_cast<std::uint32_t>(srv.Format);

        ID3D11Resource *raw = nullptr;
        view->GetResource(&raw);
        if (raw == nullptr)
            return false;

        observed.resource_key = static_cast<std::uint64_t>(
            reinterpret_cast<std::uintptr_t>(raw));
        D3D11_RESOURCE_DIMENSION type = D3D11_RESOURCE_DIMENSION_UNKNOWN;
        raw->GetType(&type);

        bool exact_cube_shape = false;
        if (type == D3D11_RESOURCE_DIMENSION_TEXTURE2D) {
            D3D11_TEXTURE2D_DESC td{};
            static_cast<ID3D11Texture2D *>(raw)->GetDesc(&td);
            observed.width = td.Width;
            observed.height = td.Height;
            observed.array_size = td.ArraySize;
            observed.mip_levels = td.MipLevels;
            observed.resource_format =
                static_cast<std::uint32_t>(td.Format);

            const bool resource_format =
                td.Format == DXGI_FORMAT_BC6H_TYPELESS ||
                td.Format == DXGI_FORMAT_BC6H_UF16 ||
                td.Format == DXGI_FORMAT_BC6H_SF16;
            const bool typed_srv =
                srv.Format == DXGI_FORMAT_BC6H_UF16 ||
                srv.Format == DXGI_FORMAT_BC6H_SF16;
            const bool full_cube_view =
                srv.ViewDimension == D3D11_SRV_DIMENSION_TEXTURECUBE &&
                srv.TextureCube.MostDetailedMip == 0u &&
                (srv.TextureCube.MipLevels == k_dsr_levels ||
                 srv.TextureCube.MipLevels == 0xffffffffu);
            exact_cube_shape =
                td.Width == k_dsr_size &&
                td.Height == k_dsr_size &&
                td.ArraySize == k_faces &&
                td.MipLevels == k_dsr_levels &&
                td.SampleDesc.Count == 1u &&
                (td.MiscFlags & D3D11_RESOURCE_MISC_TEXTURECUBE) != 0u &&
                (td.BindFlags & D3D11_BIND_SHADER_RESOURCE) != 0u &&
                resource_format && typed_srv && full_cube_view;
        }
        observed.view_shape_valid = exact_cube_shape;
        raw->Release();

        const auto view_key = static_cast<std::uint64_t>(
            reinterpret_cast<std::uintptr_t>(view));
        std::lock_guard<std::mutex> lock(g_mutex);
        const auto by_view = g_resource_by_view.find(view_key);
        observed.view_registered = by_view != g_resource_by_view.end();
        const auto by_resource =
            g_native_resources.find(observed.resource_key);
        observed.resource_registered =
            by_resource != g_native_resources.end();

        if (!observed.resource_registered)
            return false;
        observed.probe = by_resource->second.probe_ordinal;
        if (observed.view_registered &&
            (by_view->second.resource_handle != observed.resource_key ||
             by_view->second.probe_ordinal != observed.probe))
            return false;

        observed.exact_native =
            exact_cube_shape &&
            observed.probe < env::k_legacy_envspec_probe_count;
        return observed.exact_native;
    };

    bool a_ok = verify(views[0], diag.a);
    bool b_ok = true;
    if (probe_b_required)
        b_ok = verify(views[2], diag.b);
    else
        diag.b = diag.a;

    for (auto *view : views)
        if (view != nullptr)
            view->Release();

    {
        std::lock_guard<std::mutex> lock(g_mutex);
        diag.pack_ready = g_device != nullptr && g_pack_ready;
        diag.sampler_ready =
            g_sampler_ready && g_sampler.handle != 0u;
    }

    if (!a_ok || !b_ok || !diag.pack_ready || !diag.sampler_ready)
        return false;

    // The ordinal came exclusively from the verified native resource SHA.
    // Materialize the exact corresponding PTDE PackedGI probe and slot.
    resource_view ignored{};
    if (!get_ptde_cube(diag.a.probe, slot, ignored))
        return false;
    if (probe_b_required && diag.b.probe != diag.a.probe &&
        !get_ptde_cube(diag.b.probe, slot, ignored))
        return false;

    ID3D11ShaderResourceView *ptde_a = nullptr;
    ID3D11ShaderResourceView *ptde_b = nullptr;
    ID3D11SamplerState *ptde_sampler = nullptr;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        if (g_device == nullptr || !g_pack_ready ||
            !g_sampler_ready || g_sampler.handle == 0u)
            return false;

        const std::uint32_t key_a =
            static_cast<std::uint32_t>(diag.a.probe) * k_ptde_slots + slot;
        const std::uint32_t key_b =
            static_cast<std::uint32_t>(diag.b.probe) * k_ptde_slots + slot;
        const auto a = g_ptde_cubes.find(key_a);
        const auto b = g_ptde_cubes.find(key_b);
        if (a == g_ptde_cubes.end() || b == g_ptde_cubes.end() ||
            a->second.view.handle == 0u || b->second.view.handle == 0u)
            return false;

        ptde_a = reinterpret_cast<ID3D11ShaderResourceView *>(
            static_cast<std::uintptr_t>(a->second.view.handle));
        ptde_b = reinterpret_cast<ID3D11ShaderResourceView *>(
            static_cast<std::uintptr_t>(b->second.view.handle));
        ptde_sampler = reinterpret_cast<ID3D11SamplerState *>(
            static_cast<std::uintptr_t>(g_sampler.handle));
        if (ptde_a == nullptr || ptde_b == nullptr ||
            ptde_sampler == nullptr)
            return false;
        ptde_a->AddRef();
        ptde_b->AddRef();
        ptde_sampler->AddRef();
    }

    prepared.ptde_a = ptde_a;
    prepared.ptde_b = ptde_b;
    prepared.sampler = ptde_sampler;
    prepared.probe_a = diag.a.probe;
    prepared.probe_b = diag.b.probe;
    prepared.slot = slot;
    prepared.probe_b_required = probe_b_required;
    prepared.ready = true;
    diag.prepared = true;
    telemetry::hot_count(g_prepare_ok);
    return true;
}
#endif

"""

DRAW_ADD = """#if defined(DSRRL_V224_EXACT_NONMETAL_T12_RESOURCE_JOIN)
    // V223 proved shared LightBank source; V222 isolated eight native t12
    // view misses. Attempt a narrower authoritative RESOURCE join only when
    // the exact nonmetal EnvSpec draw was otherwise rejected. The underlying
    // resource must already have a native BC6H SHA256-to-probe match.
    // Original P_Metal path is never affected.
    if (!env_resource_ready && !k_native_dsr_cubemap_feed &&
        sidecar_20261010::exact_nonmetal_draw(material, decision)) {
        envspec_resource_runtime::native_resource_join_v224 resource_diag{};
        const bool recovered =
            env_resources_.prepare_exact_native_resource_join_v224(
                context, env_semantics.envspc_slot,
                probe_b_required, prepared.env_resources, resource_diag);
        env_resource_ready = recovered;

        static std::atomic<std::uint32_t> success_logged{0u};
        static std::atomic<std::uint32_t> reject_logged{0u};
        auto &logged = recovered ? success_logged : reject_logged;
        if (logged.fetch_add(1u, std::memory_order_relaxed) < 12u) {
            char line[740]{};
            std::snprintf(line, sizeof(line),
                "[DSRRL V224 NATIVE RESOURCE JOIN] "
                "rx=%u route=%u slot=%u env_slot=%u "
                "native_view=%llx resource_key=%llx "
                "view_registered=%u resource_registered=%u shape_valid=%u "
                "srv_dim=%u srv_fmt=%u texture_fmt=%u "
                "w=%u h=%u mips=%u array=%u "
                "probe_a=%u probe_b=%u exact_resource_join=%u "
                "pack_ready=%u sampler_ready=%u "
                "stage=%s pixel=OPEN",
                static_cast<unsigned>(decision.receiver_id),
                static_cast<unsigned>(material.route_index),
                static_cast<unsigned>(material.material_slot),
                static_cast<unsigned>(env_semantics.envspc_slot),
                static_cast<unsigned long long>(
                    resource_diag.a.resource_key),
                static_cast<unsigned long long>(
                    resource_diag.a.resource_key),
                resource_diag.a.view_registered ? 1u : 0u,
                resource_diag.a.resource_registered ? 1u : 0u,
                resource_diag.a.view_shape_valid ? 1u : 0u,
                resource_diag.a.srv_dimension,
                resource_diag.a.srv_format,
                resource_diag.a.resource_format,
                resource_diag.a.width, resource_diag.a.height,
                resource_diag.a.mip_levels, resource_diag.a.array_size,
                static_cast<unsigned>(resource_diag.a.probe),
                static_cast<unsigned>(resource_diag.b.probe),
                recovered ? 1u : 0u,
                resource_diag.pack_ready ? 1u : 0u,
                resource_diag.sampler_ready ? 1u : 0u,
                recovered ? "resource_join_ready" : "stock_dsr_fail_open");
            reshade::log::message(reshade::log::level::info, line);
        }
    }
#endif

"""

def replace(source: str, anchor: str, addition: str, path: str) -> str:
    if source.count(addition + anchor) == 1:
        return source
    if source.count(anchor) != 1:
        raise RuntimeError("V224 source drift: " + path +
                           " expected unique " + repr(anchor))
    return source.replace(anchor, addition + anchor, 1)

def patches(h: str, r: str, d: str) -> tuple[str, str, str]:
    if h.count(HEADER_ANCHOR) != 1 or r.count(RESOURCE_ANCHOR) != 1:
        raise RuntimeError("V224 unexpected resource ABI or operator source")
    if "    bool env_resource_ready =" not in d or d.count(DRAW_ANCHOR) != 1:
        raise RuntimeError("V224 requires generated V221/V222 EnvSpec route")
    if "V223 SHARED LIGHTBANK" not in d and "V223 SHARED LIGHTBANK" not in r:
        # V223 modifies another TU; its source existence is checked by build
        # configuration, not by guessing this draw file.
        pass
    return (
        replace(h, HEADER_ANCHOR, HEADER_ADD, "header"),
        replace(r, RESOURCE_ANCHOR, RESOURCE_ADD, "resource"),
        replace(d, DRAW_ANCHOR, DRAW_ADD, "draw"),
    )

def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--apply", action="store_true")
    args = parser.parse_args()
    old = tuple(p.read_text(encoding="utf-8").replace("\r\n", "\n")
                for p in (HEADER, RESOURCE, DRAW))
    new = patches(*old)
    if args.apply:
        for path, before, after in zip((HEADER, RESOURCE, DRAW), old, new):
            if before != after:
                path.write_text(after, encoding="utf-8", newline="\n")
    elif old != new:
        raise RuntimeError("V224 generated exact resource bridge not applied")
    print("V224_EXACT_NATIVE_T12_RESOURCE_JOIN_" +
          ("APPLIED" if args.apply else "VERIFIED"))
    print("V224_NO_GUESSED_PROBE_PMETAL_UNCHANGED_FAIL_OPEN=TRUE")
    print("V224_PIXEL_BEHAVIOR_OPEN")

if __name__ == "__main__":
    main()
