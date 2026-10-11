#!/usr/bin/env python3
"""V220: recover exact native probe identity for an alternate live D3D11 cube SRV.

Generated-source overlay applied AFTER V203/V204/V216/V219. This never
changes the original P_Metal route345 or source fallback and does NOT
invent a probe ordinal: the backing D3D11 resource must already have
matched the complete pinned native cubemap SHA-256 at resource creation.
A different SRV on that same immutable resource is an exact identity
alias only for authenticated nonmetal RX33..35 draws.
"""
import argparse
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]
HDR=ROOT/"renderer-core/include/dsrrl/runtime/envspec_resource_runtime.hpp"
RESOURCE=ROOT/"renderer-core/src/runtime/envspec_resource_runtime.cpp"
DRAW=ROOT/"renderer-core/src/runtime/pmetal_envspec_draw_runtime.cpp"

def once(src, old, new, count=1):
    if src.count(new)==count:return src
    if src.count(old)!=count:
        raise RuntimeError(f"V220 anchor drift expected {count}, got {src.count(old)}: {old[:100]!r}")
    return src.replace(old,new,count)

def header(s):
    # Only the new call path opts in; every old caller remains strict.
    a='''        prepared_envspec_resources &prepared) noexcept;'''
    b='''        prepared_envspec_resources &prepared,
        bool allow_exact_native_alias = false) noexcept;'''
    for name in ('prepare', 'prepare_bound'):
        marker = '    bool '+name+'(\n'
        begin = s.index(marker)
        end = s.index('noexcept;',begin)+len('noexcept;')
        chunk=s[begin:end]
        if chunk.count(a)!=1:raise RuntimeError('V220 declaration drift: '+name)
        s=s[:begin]+chunk.replace(a,b,1)+s[end:]
    return s

def resource(s):
    s=once(s,
        '''    bool &needs_cube) noexcept
{''',
        '''    bool &needs_cube,
    bool allow_exact_native_alias) noexcept
{''')
    s=once(s,
        '''    const auto resolve_probe_locked =
        [](ID3D11ShaderResourceView *view,
           std::uint16_t &probe) noexcept {''',
        '''    const auto resolve_probe_locked =
        [allow_exact_native_alias](ID3D11ShaderResourceView *view,
           std::uint16_t &probe) noexcept {''')
    old='''            if (by_view == g_resource_by_view.end())
                return false;

            probe =
                by_view->second.probe_ordinal;
            return true;'''
    new='''            if (by_view == g_resource_by_view.end()) {
                if (!allow_exact_native_alias)
                    return false;
                // Only an exact original native DSR 256x256 BC6H cubemap
                // whose complete bytes were SHA-256 matched in
                // on_init_resource can authorize this second SRV.
                D3D11_SHADER_RESOURCE_VIEW_DESC desc{};
                view->GetDesc(&desc);
                if (desc.ViewDimension !=
                        D3D11_SRV_DIMENSION_TEXTURECUBE ||
                    (desc.Format != DXGI_FORMAT_BC6H_UF16 &&
                     desc.Format != DXGI_FORMAT_BC6H_SF16))
                    return false;
                ID3D11Resource *backing = nullptr;
                view->GetResource(&backing);
                if (backing == nullptr)
                    return false;
                const auto backing_key =
                    static_cast<std::uint64_t>(
                        reinterpret_cast<std::uintptr_t>(backing));
                backing->Release();
                const auto native =
                    g_native_resources.find(backing_key);
                if (native == g_native_resources.end())
                    return false;
                probe = native->second.probe_ordinal;
                static std::atomic<std::uint32_t> hits{0u};
                if (hits.fetch_add(1u,
                        std::memory_order_relaxed) < 12u) {
                    char line[220]{};
                    std::snprintf(line,sizeof(line),
                        "[DSRRL V220 NONMETAL PROBE] stage=exact_native_resource_alias probe=%u source_sha256_verified=1 pmetal_route345=excluded",
                        static_cast<unsigned>(probe));
                    reshade::log::message(
                        reshade::log::level::info,line);
                }
                return true;
            }

            probe =
                by_view->second.probe_ordinal;
            return true;'''
    s=once(s,old,new)
    # Signatures of the two methods, not the independent native-DSR diagnostic.
    for name in ('prepare','prepare_bound'):
        begin=s.index('bool envspec_resource_runtime::'+name+'(')
        end=s.index('    prepared = {};',begin)
        chunk=s[begin:end]
        old_decl='    prepared_envspec_resources &prepared) noexcept'
        new_decl='    prepared_envspec_resources &prepared,\n    bool allow_exact_native_alias) noexcept'
        if chunk.count(old_decl)!=1:raise RuntimeError('V220 definition drift: '+name)
        s=s[:begin]+chunk.replace(old_decl,new_decl,1)+s[end:]
    s=once(s,
        '''            probe_b_required,
            prepared);''',
        '''            probe_b_required,
            prepared,
            allow_exact_native_alias);''')
    for indent in ('            ', '                    '):
        s=once(s,
            indent+'sampler_native,\n'+indent+'needs_cube);',
            indent+'sampler_native,\n'+indent+'needs_cube,\n'+indent+'allow_exact_native_alias);')
    return s

def draw(s):
    # This source was materialized by the V203 full-sidecar overlay.
    if 'sidecar_20261010::exact_nonmetal_draw' not in s:
        raise RuntimeError("Missing generated nonmetal draw gate")
    old='''    const bool env_resource_ready =
        k_native_dsr_cubemap_feed'''
    new='''    // P_Metal does not enter this fallback. Only the complete exact
    // nonmetal ownership/MTD/receiver gate can opt in.
    const bool v220_nonmetal_probe_alias =
        material.route_index != 345u &&
        sidecar_20261010::exact_nonmetal_draw(material, decision);
    const bool env_resource_ready =
        k_native_dsr_cubemap_feed'''
    s=once(s,old,new)
    s=once(s,
        '''                ? env_resources_.prepare_bound(
                      shadow_env[0],
                      probe_b_required
                          ? shadow_env[2]
                          : nullptr,
                      env_semantics.envspc_slot,
                      probe_b_required,
                      prepared.env_resources)''',
        '''                ? env_resources_.prepare_bound(
                      shadow_env[0],
                      probe_b_required
                          ? shadow_env[2]
                          : nullptr,
                      env_semantics.envspc_slot,
                      probe_b_required,
                      prepared.env_resources,
                      v220_nonmetal_probe_alias)''')
    s=once(s,
        '''                      probe_b_required,
                      prepared.env_resources);''',
        '''                      probe_b_required,
                      prepared.env_resources,
                      v220_nonmetal_probe_alias);''')
    return s

def main():
    p=argparse.ArgumentParser()
    p.add_argument("--apply",action="store_true")
    a=p.parse_args()
    for path,fn in ((HDR,header),(RESOURCE,resource),(DRAW,draw)):
        src=path.read_text(encoding="utf8").replace("\r\n","\n")
        dst=fn(src)
        if a.apply and dst!=src:
            path.write_text(dst,encoding="utf8",newline="\n")
        elif not a.apply and dst!=src:
            raise RuntimeError("V220 not materialized: "+str(path))
    print("V220_EXACT_NATIVE_RESOURCE_ALIAS_"+("APPLIED" if a.apply else "VERIFIED"))
    print("V220_PMETAL_ROUTE345=UNMODIFIED")
    print("V220_UNMATCHED_RESOURCE=FAIL_OPEN")
    print("V220_PTDE_PIXEL=OPEN")

if __name__=="__main__":
    main()
