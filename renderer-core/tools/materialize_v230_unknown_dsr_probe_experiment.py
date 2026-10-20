#!/usr/bin/env python3
"""V230 diagnostic ONLY: use precisely observed stock DSR BC6H SRVs on previously unmapped t12/t14.
Runs after the pinned V359 and V230 materializers, before native compilation.
No PTDE probe aliasing, no permission to promote this to release.
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CPP = ROOT / "src/runtime/envspec_resource_runtime.cpp"
HDR = ROOT / "include/dsrrl/runtime/envspec_resource_runtime.hpp"
DRAW = ROOT / "src/runtime/pmetal_envspec_draw_runtime.cpp"
FLAG = "DSRRL_V230_UNKNOWN_DSR_PROBE_DIAG"
HASHES = [
    "8dcb619433174de2ea4b9efbb9686fc25166f26ac2401cbee61e3013a40c94a5",
    "fae80a0e49fa59091bda59c4a924c1a174a87b3b8d3a03530d38ab4da4580c82",
    "e73d801d06f27070d2750c34fb2c3aef5099ab1ede206b2258aec97b1a9956c5",
]


def replace_once(src: str, anchor: str, replacement: str, label: str) -> str:
    occurrences = src.count(anchor)
    if occurrences != 1:
        raise RuntimeError(f"V230 unknown-DSR probe source drift at {label}: occurrences={occurrences}")
    return src.replace(anchor, replacement, 1)


def cubemap_hash_table() -> str:
    lines = []
    for h in HASHES:
        values = ", ".join(f"0x{n:02x}" for n in bytes.fromhex(h))
        lines.append(f"    std::array<std::uint8_t,32>{{{{{values}}}}}")
    return ",\n".join(lines)


def patch_cpp(src: str) -> str:
    src = replace_once(src, '#include <unordered_map>\n',
        '#include <unordered_map>\n#include <unordered_set>\n', "unordered set include")
    anchor = ('std::unordered_map<std::uint64_t,native_view_record>\n'
              '    g_resource_by_view;\n')
    insert = (
        f'#if defined({FLAG})\n'
        '// Explicitly observed UNMATCHED DSR hashes. Never a PTDE ordinal map.\n'
        'constexpr std::array<std::array<std::uint8_t,32>,3> k_v230_unknown_stock_shas{{\n'
        f'{cubemap_hash_table()}\n'
        '}};\n'
        'std::unordered_set<std::uint64_t> g_v230_unknown_native_resources;\n'
        'std::unordered_map<std::uint64_t,std::uint64_t> g_v230_unknown_native_views;\n'
        '#endif\n')
    src = replace_once(src, anchor, anchor+insert, "resource registries")
    anchor = '        g_resource_by_view.clear();'
    src = replace_once(src, anchor,
        anchor+f'\n#if defined({FLAG})\n'
        '        g_v230_unknown_native_resources.clear();\n'
        '        g_v230_unknown_native_views.clear();\n'
        '#endif', "device cleanup")
    anchor = '    if (!probe.has_value()) {'
    src = replace_once(src, anchor, (
        '    if (!probe.has_value()) {\n'
        f'#if defined({FLAG})\n'
        '        // Full 6-face, 8-mip creation-data SHA, NOT a filename guess.\n'
        '        const bool explicitly_observed =\n'
        '            std::find(k_v230_unknown_stock_shas.begin(),\n'
        '                      k_v230_unknown_stock_shas.end(),digest)\n'
        '            != k_v230_unknown_stock_shas.end();\n'
        '        if (explicitly_observed) {\n'
        '            std::lock_guard<std::mutex> lock(g_mutex);\n'
        '            if (g_device == device_ptr)\n'
        '                g_v230_unknown_native_resources.insert(resource_handle.handle);\n'
        '        }\n'
        '#endif\n'
        '        // Existing V357/V358 unknown-resource forensics and fail-open continue below.\n'), "unknown DSR SHA registry")
    anchor = ('    g_native_resources.erase(\n'
              '        resource_handle.handle);\n')
    src = replace_once(src, anchor, anchor+(
        f'#if defined({FLAG})\n'
        '    g_v230_unknown_native_resources.erase(resource_handle.handle);\n'
        '    for (auto it=g_v230_unknown_native_views.begin();\n'
        '         it!=g_v230_unknown_native_views.end();) {\n'
        '        if (it->second==resource_handle.handle)\n'
        '            it=g_v230_unknown_native_views.erase(it);\n'
        '        else ++it;\n'
        '    }\n'
        '#endif\n'), "resource destroy")
    anchor = ('    const auto native =\n'
              '        g_native_resources.find(\n'
              '            resource_handle.handle);')
    src = replace_once(src, anchor, (
        f'#if defined({FLAG})\n'
        '    if (g_v230_unknown_native_resources.count(resource_handle.handle)) {\n'
        '        g_v230_unknown_native_views[view.handle]=resource_handle.handle;\n'
        '        return;\n'
        '    }\n'
        '#endif\n'
        + anchor), "view registration")
    anchor = ('    if (g_resource_by_view.erase(\n'
              '            view.handle) != 0u)\n'
              '        g_snapshot_epoch.fetch_add(\n'
              '            1u,\n'
              '            std::memory_order_release);')
    src = replace_once(src, anchor, anchor+(
        f'\n#if defined({FLAG})\n'
        '    g_v230_unknown_native_views.erase(view.handle);\n'
        '#endif'), "view retirement")

    method = r"""
#if defined(DSRRL_V230_UNKNOWN_DSR_PROBE_DIAG)
// Experimental diagnostic ONLY. Known PTDE probes continue through original
// prepare_bound(), and only explicit unknown hashes are allowed to reach here.
// A/B must be the actual bound DSR t12/t14 SRVs; no guessed PTDE ordinal.
bool envspec_resource_runtime::prepare_unknown_native_dsr(
    ID3D11DeviceContext *context,
    std::uint8_t slot,
    bool probe_b_required,
    prepared_envspec_resources &prepared) noexcept
{
    prepared = {};
    if (context==nullptr || slot>=k_ptde_slots) return false;
    ID3D11ShaderResourceView *stock_views[3]{};
    context->PSGetShaderResources(12u, probe_b_required?3u:1u,stock_views);
    auto *a=stock_views[0];
    auto *b=probe_b_required?stock_views[2]:stock_views[0];
    bool ready=false;
    std::uint16_t ordinal_a=0xffffu,ordinal_b=0xffffu;
    ID3D11SamplerState *sampler_native=nullptr;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        const auto identify=[](ID3D11ShaderResourceView *srv,std::uint16_t &ordinal,bool &unknown) {
            unknown=false;
            if(!srv) return false;
            auto key=static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(srv));
            if(g_v230_unknown_native_views.count(key)) {
                ordinal=0xffffu; unknown=true; return true;
            }
            auto it=g_resource_by_view.find(key);
            if(it==g_resource_by_view.end() ||
               it->second.probe_ordinal>=env::k_legacy_envspec_probe_count) return false;
            ordinal=it->second.probe_ordinal; return true;
        };
        bool ua=false,ub=false;
        if(g_device!=nullptr && g_sampler_ready && g_sampler.handle!=0u &&
           identify(a,ordinal_a,ua) && identify(b,ordinal_b,ub) &&
           (ua || ub)) {
            sampler_native=reinterpret_cast<ID3D11SamplerState *>(
                static_cast<std::uintptr_t>(g_sampler.handle));
            if(sampler_native!=nullptr) {sampler_native->AddRef(); ready=true;}
        }
    }
    // t13 belongs to stock EnvDiffuse, never borrow or replace.
    if(stock_views[1]) stock_views[1]->Release();
    if(!ready || !a || !b) {
        if(stock_views[0]) stock_views[0]->Release();
        if(stock_views[2]) stock_views[2]->Release();
        if(sampler_native) sampler_native->Release();
        return false;
    }
    if(!probe_b_required) b->AddRef(); // Stable A==B; two owned references.
    prepared.ptde_a=a;
    prepared.ptde_b=b;
    prepared.sampler=sampler_native;
    prepared.probe_a=ordinal_a;
    prepared.probe_b=ordinal_b;
    prepared.slot=slot;
    prepared.probe_b_required=probe_b_required;
    prepared.experimental_unknown_native=true;
    prepared.ready=true;
    return true;
}
#endif

"""
    anchor='bool envspec_resource_runtime::prepare_native_dsr(\n'
    src=replace_once(src,anchor,method+anchor,"native diagnostic method")
    return src


def patch_header(src: str) -> str:
    anchor=('    bool prepare_native_dsr(\n'
            '        ID3D11DeviceContext *context,\n'
            '        std::uint8_t slot,\n'
            '        bool probe_b_required,\n'
            '        prepared_envspec_resources &prepared) noexcept;')
    insert=(f'\n#if defined({FLAG})\n'
            '    bool prepare_unknown_native_dsr(\n'
            '        ID3D11DeviceContext *context, std::uint8_t slot,\n'
            '        bool probe_b_required,\n'
            '        prepared_envspec_resources &prepared) noexcept;\n'
            '#endif')
    src=replace_once(src,anchor,anchor+insert,"public API")
    anchor='    bool ready = false;\n};\n\nstruct prepared_envdiffuse_resources'
    src=replace_once(src,anchor,
        '    bool ready = false;\n    bool experimental_unknown_native = false;\n};\n\nstruct prepared_envdiffuse_resources',
        "prepared flag")
    return src


def patch_draw(src: str) -> str:
    src=replace_once(src,
        '    const bool env_resource_ready =\n',
        '    bool env_resource_ready =\n', "resource readiness const")
    anchor='    if (!env_resource_ready) {\n'
    addition=(
        f'#if defined({FLAG})\n'
        '    // Only after PTDE atlas routing fails, never replace verified probes.\n'
        '    // This is an A/B diagnostic, NOT PTDE resource/ordinal equivalence.\n'
        '    if (!env_resource_ready && !k_native_dsr_cubemap_feed) {\n'
        '        env_resource_ready = env_resources_.prepare_unknown_native_dsr(\n'
        '            context,env_semantics.envspc_slot,probe_b_required,\n'
        '            prepared.env_resources);\n'
        '        if(env_resource_ready) {\n'
        '            static std::atomic<unsigned> once_count{0u};\n'
        '            if(once_count.fetch_add(1u,std::memory_order_relaxed)<12u) {\n'
        '                char message[350]{};\n'
        '                std::snprintf(message,sizeof(message),\n'
        '                    "[DSRRL V230 NATIVE UNKNOWN DIAG] state=PREPARED_NOT_PIXEL rx=%u route=%u material_slot=%u env_slot=%u lerp=%u a=%u b=%u resource=STOCK_DSR_BC6H action=EXPERIMENT_ONLY",\n'
        '                    decision.receiver_id,material.route_index,material.material_slot,\n'
        '                    env_semantics.envspc_slot,probe_b_required?1u:0u,\n'
        '                    prepared.env_resources.probe_a,\n'
        '                    prepared.env_resources.probe_b);\n'
        '                reshade::log::message(reshade::log::level::warning,message);\n'
        '            }\n'
        '        }\n'
        '    }\n'
        '#endif\n')
    return replace_once(src,anchor,addition+anchor,"missing-only fallback")


def main() -> None:
    targets=((CPP,patch_cpp),(HDR,patch_header),(DRAW,patch_draw))
    original={path:path.read_text(encoding="utf-8") for path,_ in targets}
    modified={path:transform(original[path]) for path,transform in targets}
    for path,content in modified.items():
        path.write_text(content,encoding="utf-8")
    print("V230 explicit 3-hash unknown DSR cubemap fallback materialized (diagnostic only)")


if __name__=="__main__":
    main()
