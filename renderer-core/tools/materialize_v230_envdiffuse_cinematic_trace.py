#!/usr/bin/env python3
"""Read-only EDC V230 cutscene source->SRVs->D3D11 native draw trace.
No changes to PTDE shader/resource operator or game exe on disk.
Runs after A14 pre-guard/request-ready materializers, before release cleanup.
"""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
trace='#include "dsrrl/runtime/envdiffuse_cinematic_trace.hpp"'
def edit(path,fn):
 p=root/path;s=p.read_text("utf8");p.write_text(fn(s),"utf8")
def one(s,a,b,what):
 if s.count(a)!=1:raise RuntimeError(f"EDC drift {what}: {s.count(a)}")
 return s.replace(a,b,1)

def runtime(s):
 s=one(s,'#include "dsrrl/runtime/pmetal_envspec_draw_runtime.hpp"',
           '#include "dsrrl/runtime/pmetal_envspec_draw_runtime.hpp"\n'+trace,'runtime include')
 anchor="    effect_latch(effect_source_ready_);"
 pre=r"""
#if defined(DSRRL_V230_ENVDIFFUSE_CINEMATIC_TRACE)
    envdiffuse_cinematic_trace::begin(
        source.bank_signature_a,source.bank_signature_b,source.serial,
        source.row_id_a,source.row_id_b,
        static_cast<unsigned>(decision.receiver_id),
        static_cast<unsigned>(decision.route_index),
        material.semantic_name_hash,material.flver_identity_hash,
        material.material_slot,material.owner_tuple_exact);
    if(envdiffuse_cinematic_trace::local.selected) {
        const auto &w=envdiffuse_cinematic_trace::local;
        char line[1024]{};
        std::snprintf(line,sizeof(line),
          "[DSRRL EDC SRC] id=%llu frame=%llu serial=%llu"
          " bankA=%016llx:%u bankB=%016llx:%u"
          " route=%u rx=%u family=%u semantic=%016llx flver=%016llx"
          " slot=%u owner_exact=%u beta=%.9g source_valid=%u"
          " diffA=%.9g,%.9g,%.9g diffB=%.9g,%.9g,%.9g"
          " specA=%.9g,%.9g,%.9g specB=%.9g,%.9g,%.9g"
          " stage=SOURCE_ONLY",
          static_cast<unsigned long long>(w.id),
          static_cast<unsigned long long>(w.frame),
          static_cast<unsigned long long>(w.serial),
          static_cast<unsigned long long>(w.bank_a),w.row_a,
          static_cast<unsigned long long>(w.bank_b),w.row_b,
          w.route,w.rx,static_cast<unsigned>(family),
          static_cast<unsigned long long>(w.sem),
          static_cast<unsigned long long>(w.flver),w.slot,w.owner,
          static_cast<double>(source.beta),
          source.envdiffuse_linear_valid?1u:0u,
          static_cast<double>(source.envdiffuse_a[0]),
          static_cast<double>(source.envdiffuse_a[1]),
          static_cast<double>(source.envdiffuse_a[2]),
          static_cast<double>(source.envdiffuse_b[0]),
          static_cast<double>(source.envdiffuse_b[1]),
          static_cast<double>(source.envdiffuse_b[2]),
          static_cast<double>(source.a[0]),
          static_cast<double>(source.a[1]),
          static_cast<double>(source.a[2]),
          static_cast<double>(source.b[0]),
          static_cast<double>(source.b[1]),
          static_cast<double>(source.b[2]));
        reshade::log::message(reshade::log::level::warning,line);
    }
#endif
"""
 s=one(s,anchor,pre+anchor,"source witness")
 # This read-only witness executes BEFORE the EnvSpec t12 rejection exits
 # and therefore shows independently certified native EnvDiffuse t11/t13.
 # It does NOT activate the PTDE receiver, change GPU state, or bypass t12.
 rejected_t12_probe=r"""
#if defined(DSRRL_V230_ENVDIFFUSE_CINEMATIC_TRACE)
        static std::array<std::atomic<std::uint32_t>, 1024> g_v359_t11_reject_by_route{};
        const auto route_index=static_cast<std::size_t>(
            decision.route_index<1023u?decision.route_index:1023u);
        const auto nth=g_v359_t11_reject_by_route[route_index].fetch_add(
            1u,std::memory_order_relaxed)+1u;
        if(nth<=4u || ((nth & (nth-1u))==0u)) {
            const bool need_b=family==pmetal_envspec_receiver_family::hemenvlerp;
            ID3D11ShaderResourceView *diffuse[3]{};
            const unsigned n=need_b?3u:1u;
            const bool shadow=pixel_srv_shadow_snapshot(
                cmd_list,11u,n,diffuse);
            if(!shadow) {
                for(auto *&v:diffuse) {
                    if(v!=nullptr) v->Release();
                    v=nullptr;
                }
                context->PSGetShaderResources(11u,n,diffuse);
            }
            std::uint16_t a=0,b=0;
            const bool a_ok=env_resources_.native_envdiffuse_probe_for_view(
                diffuse[0],a);
            const bool b_ok=need_b &&
                env_resources_.native_envdiffuse_probe_for_view(diffuse[2],b);
            char log[460]{};
            std::snprintf(log,sizeof(log),
                "[DSRRL V359 T11 REJECT JOIN]"
                " rx=%u route=%u slot=%u n=%u"
                " t12=REJECTED t11=%016llx id_a=%u probe_a=%u"
                " t13=%016llx needs_b=%u id_b=%u probe_b=%u"
                " state=%s action=STOCK_DSR pixels=OPEN",
                static_cast<unsigned>(decision.receiver_id),
                static_cast<unsigned>(decision.route_index),
                static_cast<unsigned>(material.material_slot),nth,
                static_cast<unsigned long long>(
                    reinterpret_cast<std::uintptr_t>(diffuse[0])),
                a_ok?1u:0u,static_cast<unsigned>(a),
                static_cast<unsigned long long>(
                    reinterpret_cast<std::uintptr_t>(diffuse[2])),
                need_b?1u:0u,b_ok?1u:0u,
                static_cast<unsigned>(b),shadow?"SHADOW":"D3D11");
            reshade::log::message(reshade::log::level::warning,log);
            // Read-only SRV dimension and native resource type witness.
            if(diffuse[0]!=nullptr) {
                D3D11_SHADER_RESOURCE_VIEW_DESC vd{};
                diffuse[0]->GetDesc(&vd);
                ID3D11Resource *res=nullptr;
                diffuse[0]->GetResource(&res);
                D3D11_RESOURCE_DIMENSION dim=D3D11_RESOURCE_DIMENSION_UNKNOWN;
                if(res!=nullptr) res->GetType(&dim);
                D3D11_TEXTURE2D_DESC tex{};
                bool tex2d=false;
                if(res!=nullptr && dim==D3D11_RESOURCE_DIMENSION_TEXTURE2D) {
                    ID3D11Texture2D *texture=nullptr;
                    if(SUCCEEDED(res->QueryInterface(
                        __uuidof(ID3D11Texture2D),
                        reinterpret_cast<void **>(&texture))) && texture!=nullptr) {
                        texture->GetDesc(&tex);
                        tex2d=true;
                        texture->Release();
                    }
                }
                const bool candidate=tex2d && tex.Width==64u && tex.Height==64u &&
                    tex.MipLevels==1u && tex.ArraySize==6u &&
                    tex.SampleDesc.Count==1u &&
                    (tex.Format==DXGI_FORMAT_BC6H_TYPELESS ||
                     tex.Format==DXGI_FORMAT_BC6H_UF16 ||
                     tex.Format==DXGI_FORMAT_BC6H_SF16);
                char detail[800]{};
                std::snprintf(detail,sizeof(detail),
                    "[DSRRL V359 T11 BOUND DESC] rx=%u route=%u"
                    " srv_dim=%u srv_fmt=%u resource_dim=%u"
                    " backing=%016llx tex2d=%u w=%u h=%u mips=%u"
                    " array=%u tex_fmt=%u sample_count=%u"
                    " create_desc_candidate=%u action=READ_ONLY",
                    static_cast<unsigned>(decision.receiver_id),
                    static_cast<unsigned>(decision.route_index),
                    static_cast<unsigned>(vd.ViewDimension),
                    static_cast<unsigned>(vd.Format),
                    static_cast<unsigned>(dim),
                    static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(res)),
                    tex2d?1u:0u,tex.Width,tex.Height,tex.MipLevels,
                    tex.ArraySize,static_cast<unsigned>(tex.Format),
                    tex.SampleDesc.Count,candidate?1u:0u);
                reshade::log::message(reshade::log::level::warning,detail);
                if(res!=nullptr) res->Release();
            }
            for(auto *v:diffuse)
                if(v!=nullptr) v->Release();
        }
#endif
"""
 s=one(s,"    if (!env_resource_ready) {",
       "    if (!env_resource_ready) {\n"+rejected_t12_probe,
       "v359 t11 independent witness on t12 rejection")

 anchor="    effect_latch(effect_request_ready_);"
 post=r"""
#if defined(DSRRL_V230_ENVDIFFUSE_CINEMATIC_TRACE)
    if(envdiffuse_cinematic_trace::local.source) {
        auto &w=envdiffuse_cinematic_trace::local;
        w.ready=true;
        envdiffuse_cinematic_trace::ready_count.fetch_add(
            1u,std::memory_order_relaxed);
        if(w.selected) {
            ID3D11ShaderResourceView *original_t11=nullptr,*original_t13=nullptr;
            ID3D11SamplerState *original_s11=nullptr,*original_s13=nullptr;
            context->PSGetShaderResources(11u,1u,&original_t11);
            context->PSGetShaderResources(13u,1u,&original_t13);
            context->PSGetSamplers(11u,1u,&original_s11);
            context->PSGetSamplers(13u,1u,&original_s13);
            char line[1100]{};
            std::snprintf(line,sizeof(line),
               "[DSRRL EDC READY] id=%llu frame=%llu serial=%llu"
               " row=%u:%u route=%u rx=%u family=%u beta=%.9g"
               " shader=%u cb12=%u specA=%u specB=%u diffA=%u diffB=%u"
               " ptde_sampler=%u stock_t11=%016llx stock_t13=%016llx"
               " stock_s11=%016llx stock_s13=%016llx"
               " cb12_c86A=%.9g,%.9g,%.9g"
               " cb12_c86B=%.9g,%.9g,%.9g"
               " mutation_srvs=%u mutation_samplers=%u"
               " stage=REQUEST_READY_NATIVE_DRAW_NOT_YET_PROVEN",
               static_cast<unsigned long long>(w.id),
               static_cast<unsigned long long>(w.frame),
               static_cast<unsigned long long>(w.serial),
               w.row_a,w.row_b,w.route,w.rx,static_cast<unsigned>(family),
               static_cast<double>(source.beta),
               prepared.shader!=nullptr?1u:0u,prepared.b12!=nullptr?1u:0u,
               prepared.env_resources.ptde_a!=nullptr?1u:0u,
               prepared.env_resources.ptde_b!=nullptr?1u:0u,
               prepared.envdiffuse_resources.ptde_a!=nullptr?1u:0u,
               prepared.envdiffuse_resources.ptde_b!=nullptr?1u:0u,
               prepared.envdiffuse_resources.sampler!=nullptr?1u:0u,
               static_cast<unsigned long long>(
                   reinterpret_cast<std::uintptr_t>(original_t11)),
               static_cast<unsigned long long>(
                   reinterpret_cast<std::uintptr_t>(original_t13)),
               static_cast<unsigned long long>(
                   reinterpret_cast<std::uintptr_t>(original_s11)),
               static_cast<unsigned long long>(
                   reinterpret_cast<std::uintptr_t>(original_s13)),
               static_cast<double>(source.envdiffuse_a[0]),
               static_cast<double>(source.envdiffuse_a[1]),
               static_cast<double>(source.envdiffuse_a[2]),
               static_cast<double>(source.envdiffuse_b[0]),
               static_cast<double>(source.envdiffuse_b[1]),
               static_cast<double>(source.envdiffuse_b[2]),
               prepared.request.srv_count,prepared.request.sampler_count);
            reshade::log::message(reshade::log::level::warning,line);
            if(original_t11)original_t11->Release();
            if(original_t13)original_t13->Release();
            if(original_s11)original_s11->Release();
            if(original_s13)original_s13->Release();
        }
    }
#endif
"""
 return one(s,anchor,anchor+"\n"+post,"post guard")

edit("src/runtime/pmetal_envspec_draw_runtime.cpp",runtime)

def integrated(s):
 s=one(s,'#include "dsrrl/runtime/flver_identity_transport.hpp"',
           '#include "dsrrl/runtime/flver_identity_transport.hpp"\n'+trace,'integrated include')
 s=one(s,'    if (exact_pmetal_envspec) {\n',
   '''    if (exact_pmetal_envspec) {
#if defined(DSRRL_V230_ENVDIFFUSE_CINEMATIC_TRACE)
        dsrrl::runtime::envdiffuse_cinematic_trace::reset();
#endif
''',"trace lifecycle entry")
 # The single shared preparation path handles draw and draw_indexed.
 a='''            return false;
        }

#if defined(DSRRL_V285_ATOMIC_NONMETAL_DIAG)
        if (material.route_index != 345u) {'''
 b='''#if defined(DSRRL_V230_ENVDIFFUSE_CINEMATIC_TRACE)
            const auto &w=dsrrl::runtime::envdiffuse_cinematic_trace::local;
            if(w.source) {
                dsrrl::runtime::envdiffuse_cinematic_trace::
                    prepare_fail.fetch_add(1u,std::memory_order_relaxed);
                if(w.selected) {
                    char line[300]{};
                    std::snprintf(line,sizeof(line),
                        "[DSRRL EDC FAILOPEN] id=%llu frame=%llu serial=%llu"
                        " row=%u:%u route=%u rx=%u"
                        " phase=POST_SOURCE_PREPARE_REJECT native_stock_draw=1",
                        static_cast<unsigned long long>(w.id),
                        static_cast<unsigned long long>(w.frame),
                        static_cast<unsigned long long>(w.serial),
                        w.row_a,w.row_b,w.route,w.rx);
                    reshade::log::message(reshade::log::level::warning,line);
                }
            }
#endif
            return false;
        }

#if defined(DSRRL_V285_ATOMIC_NONMETAL_DIAG)
        if (material.route_index != 345u) {'''
 s=one(s,a,b,"failopen after source")
 s=one(s,"void on_present(\n","void on_present(\n","present signature unique noop") if False else s
 anchor="    g_clustered_pnts.frame_event(present);"
 s=one(s,anchor,anchor+'''
#if defined(DSRRL_V230_ENVDIFFUSE_CINEMATIC_TRACE)
    dsrrl::runtime::envdiffuse_cinematic_trace::frame.store(
        present,std::memory_order_relaxed);
    if((present%600u)==0u) {
        char line[480]{};
        std::snprintf(line,sizeof(line),
          "[DSRRL EDC HEARTBEAT] frame=%llu src=%llu ready=%llu"
          " native_ok=%llu native_fail=%llu post_source_failopen=%llu",
          static_cast<unsigned long long>(present),
          static_cast<unsigned long long>(
              dsrrl::runtime::envdiffuse_cinematic_trace::observed.load()),
          static_cast<unsigned long long>(
              dsrrl::runtime::envdiffuse_cinematic_trace::ready_count.load()),
          static_cast<unsigned long long>(
              dsrrl::runtime::envdiffuse_cinematic_trace::native_ok.load()),
          static_cast<unsigned long long>(
              dsrrl::runtime::envdiffuse_cinematic_trace::native_fail.load()),
          static_cast<unsigned long long>(
              dsrrl::runtime::envdiffuse_cinematic_trace::prepare_fail.load()));
        reshade::log::message(reshade::log::level::warning,line);
    }
#endif
''',"frame heartbeat")
 return s
edit("integrated/integrated_addon.cpp",integrated)

def native(s):
 s=one(s,'#include "dsrrl/runtime/d3d11_cb_window.hpp"',
           '#include "dsrrl/runtime/d3d11_cb_window.hpp"\n'+trace,'native include')
 # Actual original draw is between capture/restore, not at ReShade request-ready.
 # Write after native restore; do not modify any pending fields.
 signature='''    if (!restore_state(
            context,
            pending.context1,
            state)) {'''
 found=s.count(signature)
 if found!=4:raise RuntimeError(f"native restore sites drift {found}")
 s=s.replace(signature,'''    const bool edc_restored = restore_state(
            context,
            pending.context1,
            state);
    if (!edc_restored) {''')
 # Native hooks all 4 end with release. Make log only for exact m14/s14
 # matching thread-local source ID. The native bridge can span two stages on
 # separate threads; missing join is explicitly noted, not guessed.
 a='''    release_pending_draw(
        pending);
}'''
 if s.count(a)!=4:raise RuntimeError(f"native hook tail drift {s.count(a)}")
 b=r'''#if defined(DSRRL_V230_ENVDIFFUSE_CINEMATIC_TRACE)
    {
        const auto &w=envdiffuse_cinematic_trace::local;
        const bool envdiff_owner=(pending.mutation.owners &
            core::operator_bit(core::operator_id::env_diffuse))!=0u;
        if(w.source && w.selected) {
            if(edc_restored)envdiffuse_cinematic_trace::native_ok.fetch_add(
                1u,std::memory_order_relaxed);
            else envdiffuse_cinematic_trace::native_fail.fetch_add(
                1u,std::memory_order_relaxed);
            char line[470]{};
            std::snprintf(line,sizeof(line),
              "[DSRRL EDC NATIVE] id=%llu frame=%llu serial=%llu"
              " route=%u rx=%u issued_original=1 mutation_applied=1"
              " restored=%u envdiff_owner=%u"
              " ps_swap=%u cb=%u srv=%u sampler=%u"
              " pixel=NOT_MEASURED",
              static_cast<unsigned long long>(w.id),
              static_cast<unsigned long long>(w.frame),
              static_cast<unsigned long long>(w.serial),
              w.route,w.rx,edc_restored?1u:0u,envdiff_owner?1u:0u,
              pending.mutation.replace_pixel_shader?1u:0u,
              pending.mutation.constant_buffer_count,
              pending.mutation.srv_count,
              pending.mutation.sampler_count);
            reshade::log::message(reshade::log::level::warning,line);
        }
    }
#endif
    release_pending_draw(
        pending);
}'''
 s=s.replace(a,b)
 return s
edit("src/runtime/pmetal_native_draw_bridge.cpp",native)
print("EDC_CINEMATIC_TRACE_PATCH_PASS")
