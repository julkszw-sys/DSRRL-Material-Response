#pragma once
// V419: read-only *after begin / before DrawIndexed* D3D11 state census.
// All identity/caller policy is verified by the integrated RX34 caller.
// No shader, CB, resource, render target or draw state is modified here.
#include <Windows.h>
#include <d3d11.h>
#include <reshade.hpp>
#include <atomic>
#include <cstdint>
#include <cstdio>

namespace dsrrl::runtime::v419 {
struct request {
    bool armed=false;
    void *native_context=nullptr;
    std::uintptr_t context_owner=0,model=0,fg_parent=0;
    std::uint32_t registry=0,material_slot=0,row=0;
    std::uint64_t native_producer_seq=0,source_epoch=0;
    ID3D11PixelShader *expected_shader=nullptr;
    ID3D11Buffer *expected_cb12=nullptr;
};
struct snapshot {
    bool captured=false,context_mismatch=false;
    bool deferred=false;
    std::uintptr_t actual_shader=0,cb12=0;
    std::uintptr_t rtv0=0,rtv1=0,rtv2=0,rtv3=0,dsv=0;
    std::uintptr_t index_buffer=0;
    std::uint32_t index_format=0,index_offset=0;
    std::uint32_t cb12_bytes=0,rtv0_format=0,rtv0_dimension=0;
    std::uint32_t rtv0_width=0,rtv0_height=0,rtv0_samples=0;
    std::uint32_t dsv_format=0,viewport_count=0;
    float viewport_width=0.0f,viewport_height=0.0f;
};
inline thread_local request pending{};
inline thread_local snapshot observed{};
inline std::atomic<std::uint64_t> requests{0};

inline bool arm(const request &r) noexcept {
    if (pending.armed || !r.native_context || !r.model ||
        !r.fg_parent || !r.native_producer_seq || !r.expected_shader)
        return false;
    const auto n=requests.fetch_add(1u,std::memory_order_relaxed)+1u;
    if(n>256u && n%8192u!=0u) return false;
    pending=r;
    pending.armed=true;
    observed={};
    return true;
}
inline void before_native_draw(ID3D11DeviceContext *ctx) noexcept {
    if(!pending.armed || observed.captured)return;
    if(ctx==nullptr || ctx!=pending.native_context){
        observed.context_mismatch=true;
        return;
    }
    observed.captured=true;
    observed.deferred=ctx->GetType()==D3D11_DEVICE_CONTEXT_DEFERRED;

    ID3D11PixelShader *ps=nullptr;
    ID3D11ClassInstance *classes[1]{};
    UINT nclasses=1u;
    ctx->PSGetShader(&ps,classes,&nclasses);
    observed.actual_shader=reinterpret_cast<std::uintptr_t>(ps);
    if(ps)ps->Release();
    if(classes[0])classes[0]->Release();

    ID3D11Buffer *cb12=nullptr;
    ctx->PSGetConstantBuffers(12u,1u,&cb12);
    observed.cb12=reinterpret_cast<std::uintptr_t>(cb12);
    if(cb12) {
        D3D11_BUFFER_DESC desc{};
        cb12->GetDesc(&desc);
        observed.cb12_bytes=desc.ByteWidth;
        cb12->Release();
    }
    ID3D11RenderTargetView *rtvs[4]{};
    ID3D11DepthStencilView *dsv=nullptr;
    ctx->OMGetRenderTargets(4u,rtvs,&dsv);
    observed.rtv0=reinterpret_cast<std::uintptr_t>(rtvs[0]);
    observed.rtv1=reinterpret_cast<std::uintptr_t>(rtvs[1]);
    observed.rtv2=reinterpret_cast<std::uintptr_t>(rtvs[2]);
    observed.rtv3=reinterpret_cast<std::uintptr_t>(rtvs[3]);
    observed.dsv=reinterpret_cast<std::uintptr_t>(dsv);
    if(rtvs[0]){
        D3D11_RENDER_TARGET_VIEW_DESC desc{};
        rtvs[0]->GetDesc(&desc);
        observed.rtv0_format=static_cast<std::uint32_t>(desc.Format);
        observed.rtv0_dimension=static_cast<std::uint32_t>(desc.ViewDimension);
        ID3D11Resource *resource=nullptr;
        rtvs[0]->GetResource(&resource);
        if(resource){
            ID3D11Texture2D *tex=nullptr;
            if(SUCCEEDED(resource->QueryInterface(
                __uuidof(ID3D11Texture2D),
                reinterpret_cast<void **>(&tex))) && tex){
                D3D11_TEXTURE2D_DESC td{};
                tex->GetDesc(&td);
                observed.rtv0_width=td.Width;
                observed.rtv0_height=td.Height;
                observed.rtv0_samples=td.SampleDesc.Count;
                tex->Release();
            }
            resource->Release();
        }
    }
    if(dsv){
        D3D11_DEPTH_STENCIL_VIEW_DESC desc{};
        dsv->GetDesc(&desc);
        observed.dsv_format=static_cast<std::uint32_t>(desc.Format);
        dsv->Release();
    }
    for(auto *rtv:rtvs)if(rtv)rtv->Release();

    ID3D11Buffer *ib=nullptr;
    DXGI_FORMAT fmt=DXGI_FORMAT_UNKNOWN;
    UINT offset=0u;
    ctx->IAGetIndexBuffer(&ib,&fmt,&offset);
    observed.index_buffer=reinterpret_cast<std::uintptr_t>(ib);
    observed.index_format=static_cast<std::uint32_t>(fmt);
    observed.index_offset=offset;
    if(ib)ib->Release();

    D3D11_VIEWPORT vp{};
    UINT vp_count=1u;
    ctx->RSGetViewports(&vp_count,&vp);
    observed.viewport_count=vp_count;
    if(vp_count>0u) {
        observed.viewport_width=vp.Width;
        observed.viewport_height=vp.Height;
    }
}
inline void finish(bool issued,
    std::uint32_t index_count,std::uint32_t instance_count,
    std::uint32_t first_index,std::int32_t vertex_offset,
    std::uint32_t first_instance) noexcept {
    if(!pending.armed)return;
    const auto p=pending;
    const auto s=observed;
    pending={};observed={};
    char line[1050]{};
    std::snprintf(line,sizeof(line),
        "[DSRRL V419 RX34 PRE-DRAW NATIVE STATE] "
        "owner=%p model=%p fg_parent=%p registry=%08x "
        "row=%u slot=%u native_producer_seq=%llu epoch=%llu "
        "native_ctx=%p captured=%u mismatch=%u issued=%u deferred=%u "
        "expected_ps=%p bound_ps=%p ps_match=%u "
        "expected_cb12=%p bound_cb12=%p cb12_match=%u cb12_bytes=%u "
        "rtv0=%p rtv1=%p rtv2=%p rtv3=%p dsv=%p "
        "rtv0_fmt=%u rtv0_dim=%u size=%ux%u samples=%u dsv_fmt=%u "
        "ib=%p index_format=%u index_offset=%u "
        "viewport=%.0fx%.0f viewport_count=%u "
        "index_count=%u instances=%u first_index=%u "
        "vertex_offset=%d first_instance=%u "
        "native_draw_call=CAPTURED gpu_execution=OPEN pixel=OPEN",
        reinterpret_cast<void*>(p.context_owner),
        reinterpret_cast<void*>(p.model),
        reinterpret_cast<void*>(p.fg_parent),p.registry,p.row,p.material_slot,
        static_cast<unsigned long long>(p.native_producer_seq),
        static_cast<unsigned long long>(p.source_epoch),
        p.native_context,s.captured?1u:0u,s.context_mismatch?1u:0u,
        issued?1u:0u,s.deferred?1u:0u,
        p.expected_shader,
        reinterpret_cast<void*>(s.actual_shader),
        s.captured && s.actual_shader==
            reinterpret_cast<std::uintptr_t>(p.expected_shader)?1u:0u,
        p.expected_cb12,reinterpret_cast<void*>(s.cb12),
        s.captured && p.expected_cb12!=nullptr &&
            s.cb12==reinterpret_cast<std::uintptr_t>(p.expected_cb12)?1u:0u,
        s.cb12_bytes,reinterpret_cast<void*>(s.rtv0),
        reinterpret_cast<void*>(s.rtv1),
        reinterpret_cast<void*>(s.rtv2),
        reinterpret_cast<void*>(s.rtv3),
        reinterpret_cast<void*>(s.dsv),
        s.rtv0_format,s.rtv0_dimension,
        s.rtv0_width,s.rtv0_height,s.rtv0_samples,s.dsv_format,
        reinterpret_cast<void*>(s.index_buffer),
        s.index_format,s.index_offset,s.viewport_width,
        s.viewport_height,s.viewport_count,
        index_count,instance_count,first_index,
        vertex_offset,first_instance);
    reshade::log::message(reshade::log::level::warning,line);
}
} // namespace dsrrl::runtime::v419
