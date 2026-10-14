#!/usr/bin/env python3
"""V274 isolated readback witness: actual physical Lerp PS b0 workflow / t1+t10.
V272 shader/DXBC/sidecar/endpoint algorithms are BYTE UNCHANGED.
Temporary instrumentation injected into V272 draw-runtime source only inside
the one-addon Windows build and restored to exact HEAD afterwards.
NO instrumentation whatsoever during Stable HemEnv.
"""
import argparse
from pathlib import Path

P=Path("renderer-core/src/runtime/pmetal_envspec_draw_runtime.cpp")
HEADER='#include <d3d11.h>'
INSERT_HEADER='#include <d3d11.h>\n#include <d3d11_1.h>'
BEFORE='struct f4 {\n    float x;\n    float y;\n    float z;\n    float w;\n};\n'
AFTER='''    prepared.ready = true;
    effect_latch(effect_request_ready_);
    telemetry::hot_count(requests_);
'''
CALL='''    prepared.ready = true;
    effect_latch(effect_request_ready_);
    telemetry::hot_count(requests_);
    // V274 source-scoped Lerp-only readback. Stable A/B never enter this
    // conditional and all renderer shader equations remain exactly V272.
    if (family == pmetal_envspec_receiver_family::hemenvlerp)
        v274_capture_actual_lerp_cb(
            context, material, decision, prepared,
            source.beta, env_semantics.envspc_slot);
'''
MARKER='[DSRRL V274 ACTUAL LERP CB0]'

HELPER=r'''
// V274: read-only, limited-sample physical GPU constant-buffer measurement.
// NO shader patch, PSSet* call, draw replay or EXE modification.
// Source is the currently bound b0 used by stock DSR MaterialWorkflow;
// it is NOT b12 PTDE material payload generated below.
void v274_capture_actual_lerp_cb(
    ID3D11DeviceContext *context,
    const mr::material_identity &material,
    const mr::decision &decision,
    const prepared_pmetal_envspec_draw &prepared,
    float beta,
    std::uint32_t env_slot) noexcept
{
    if (context == nullptr || !prepared.ready ||
        !material.valid || !material.owner_tuple_exact ||
        !material.material_slot_valid ||
        !decision.active || decision.receiver_id < 33u ||
        decision.receiver_id > 35u ||
        !std::isfinite(beta) || beta <= 0.0f || beta >= 1.0f ||
        env_slot > 2u)
        return;

    // Log each distinct equipment identity + material + receiver + beta
    // bucket once. No repeated GPU readbacks in the draw hot loop.
    const auto bucket = static_cast<std::uint32_t>(beta*4.0f) < 4u
                          ? static_cast<std::uint32_t>(beta*4.0f) : 3u;
    std::uint64_t key =
        material.semantic_name_hash ^
        (material.flver_identity_hash * 0x9e3779b97f4a7c15ull) ^
        (static_cast<std::uint64_t>(material.route_index + 1u) << 42u) ^
        (static_cast<std::uint64_t>(material.material_slot + 1u) << 30u) ^
        (static_cast<std::uint64_t>(decision.receiver_id) << 20u) ^
        (static_cast<std::uint64_t>(bucket) << 4u) ^
        static_cast<std::uint64_t>(env_slot);
    if (key == 0u) key = 1u;
    static std::array<std::atomic<std::uint64_t>,96> seen{};
    bool first = false;
    for (auto &slot : seen) {
        auto old = slot.load(std::memory_order_acquire);
        if (old == key)
            return;
        if (old == 0u &&
            slot.compare_exchange_strong(old,key,
                std::memory_order_acq_rel,std::memory_order_acquire)) {
            first = true;
            break;
        }
    }
    if (!first) return;

    ID3D11ShaderResourceView *stock_t1 = nullptr;
    context->PSGetShaderResources(1u,1u,&stock_t1);
    D3D11_SHADER_RESOURCE_VIEW_DESC t1_desc{};
    if (stock_t1 != nullptr) stock_t1->GetDesc(&t1_desc);
    ID3D11ShaderResourceView *ptde_t10 = nullptr;
    std::uint32_t t10_count = 0u;
    for (std::uint32_t i=0u;
         i < prepared.material_resources.request_count; ++i) {
        const auto &req=prepared.material_resources.requests[i];
        for (std::uint32_t j=0u; j<req.srv_count; ++j) {
            if (req.srvs[j].slot == 10u &&
                req.srvs[j].srv != nullptr) {
                ptde_t10 = req.srvs[j].srv;
                ++t10_count;
            }
        }
    }
    D3D11_SHADER_RESOURCE_VIEW_DESC t10_desc{};
    if (ptde_t10 != nullptr) ptde_t10->GetDesc(&t10_desc);

    // D3D11.1 PSSetConstantBuffers1 can bind a subrange. Query the actual
    // firstConstant/numConstants, not a guessed zero-based CPU mirror.
    ID3D11Buffer *cb0=nullptr;
    ID3D11DeviceContext1 *ctx1=nullptr;
    UINT first_constant=0u, num_constants=0u;
    bool has_context1 = false;
    if (SUCCEEDED(context->QueryInterface(
            __uuidof(ID3D11DeviceContext1),
            reinterpret_cast<void **>(&ctx1))) &&
        ctx1 != nullptr) {
        has_context1 = true;
        ctx1->PSGetConstantBuffers1(
            0u,1u,&cb0,&first_constant,&num_constants);
        ctx1->Release();
    } else {
        context->PSGetConstantBuffers(0u,1u,&cb0);
    }

    const char *reason="NOT_ATTEMPTED";
    float cb10[3]{}, cb85=0.0f, cb100w=0.0f, cb102=0.0f;
    std::uint32_t physical_size=0u;
    bool read_ok=false;
    if (context->GetType() != D3D11_DEVICE_CONTEXT_IMMEDIATE) {
        reason="DEFERRED_CONTEXT_NO_MAP";
    } else if (cb0 == nullptr) {
        reason="B0_UNBOUND";
    } else {
        D3D11_BUFFER_DESC src{};
        cb0->GetDesc(&src);
        physical_size=src.ByteWidth;
        // shader-visible cb0[102] needs 103 float4 registers. Ensure the
        // entire inspected window is within physical buffer and bound range.
        const std::uint64_t needed =
            (static_cast<std::uint64_t>(first_constant)+103u)*16u;
        if ((src.BindFlags & D3D11_BIND_CONSTANT_BUFFER)==0u ||
            needed > src.ByteWidth ||
            (has_context1 && num_constants != 0u &&
             num_constants < 103u)) {
            reason="B0_WINDOW_NOT_READABLE";
        } else {
            ID3D11Device *device=nullptr;
            context->GetDevice(&device);
            if (device == nullptr) {
                reason="DEVICE_ABSENT";
            } else {
                D3D11_BUFFER_DESC stage=src;
                stage.Usage=D3D11_USAGE_STAGING;
                stage.BindFlags=0u;
                stage.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
                stage.MiscFlags=0u;
                stage.StructureByteStride=0u;
                ID3D11Buffer *copy=nullptr;
                if (FAILED(device->CreateBuffer(&stage,nullptr,&copy)) ||
                    copy == nullptr) {
                    reason="STAGING_CREATE_FAILED";
                } else {
                    context->CopyResource(copy,cb0);
                    D3D11_MAPPED_SUBRESOURCE mapped{};
                    if (FAILED(context->Map(copy,0u,D3D11_MAP_READ,0u,
                                            &mapped)) ||
                        mapped.pData == nullptr) {
                        reason="STAGING_MAP_FAILED";
                    } else {
                        const auto *bytes=static_cast<const std::uint8_t *>(
                            mapped.pData);
                        const auto value=[&](std::uint32_t r,
                                             std::uint32_t lane) noexcept {
                            float f=0.0f;
                            const auto at=
                                (static_cast<std::size_t>(first_constant)+r)
                                *16u+lane*4u;
                            std::memcpy(&f,bytes+at,sizeof(f));
                            return f;
                        };
                        cb10[0]=value(10u,0u);
                        cb10[1]=value(10u,1u);
                        cb10[2]=value(10u,2u);
                        cb85=value(85u,0u);
                        cb100w=value(100u,3u);
                        cb102=value(102u,0u);
                        read_ok=true;
                        reason="GPU_CB_READ_PASS";
                        context->Unmap(copy,0u);
                    }
                    copy->Release();
                }
                device->Release();
            }
        }
    }
    if (cb0 != nullptr) cb0->Release();
    if (stock_t1 != nullptr) stock_t1->Release();

    char line[1370]{};
    std::snprintf(line,sizeof(line),
        "[DSRRL V274 ACTUAL LERP CB0] "
        "rx=%u route=%u mtd_hash=%016llx flver=%016llx "
        "slot=%u envslot=%u beta=%.7g bucket=%u "
        "c101=%.7g c100=%.7g,%.7g,%.7g "
        "ptde_specpower=%.7g "
        "stock_t1=%u t1_fmt=%u t1_dim=%u "
        "ptde_t10=%u t10_count=%u t10_fmt=%u t10_dim=%u "
        "sidecar_spec=%u diffuse=%u normal=%u "
        "cb0_read=%u reason=%s ctx1=%u b0_first=%u b0_num=%u "
        "b0_bytes=%u cb85_MaterialWorkflow=%.9g "
        "cb10_specRGB=%.9g,%.9g,%.9g "
        "cb100w_PBL=%.9g cb102_debug=%.9g "
        "stage=PREPARED_ONLY pixel=OPEN",
        static_cast<unsigned>(decision.receiver_id),
        static_cast<unsigned>(material.route_index),
        static_cast<unsigned long long>(material.semantic_name_hash),
        static_cast<unsigned long long>(material.flver_identity_hash),
        static_cast<unsigned>(material.material_slot),
        static_cast<unsigned>(env_slot),
        static_cast<double>(beta),
        static_cast<unsigned>(bucket),
        static_cast<double>(decision.c101),
        static_cast<double>(decision.c100[0]),
        static_cast<double>(decision.c100[1]),
        static_cast<double>(decision.c100[2]),
        static_cast<double>(decision.ptde_specular_power),
        stock_t1!=nullptr ? 1u : 0u,
        static_cast<unsigned>(t1_desc.Format),
        static_cast<unsigned>(t1_desc.ViewDimension),
        ptde_t10!=nullptr ? 1u : 0u,
        static_cast<unsigned>(t10_count),
        static_cast<unsigned>(t10_desc.Format),
        static_cast<unsigned>(t10_desc.ViewDimension),
        prepared.material_resources.spec_rgb ? 1u : 0u,
        prepared.material_resources.diffuse ? 1u : 0u,
        prepared.material_resources.normal ? 1u : 0u,
        read_ok ? 1u : 0u,
        reason,
        has_context1 ? 1u : 0u,
        static_cast<unsigned>(first_constant),
        static_cast<unsigned>(num_constants),
        static_cast<unsigned>(physical_size),
        static_cast<double>(cb85),
        static_cast<double>(cb10[0]),
        static_cast<double>(cb10[1]),
        static_cast<double>(cb10[2]),
        static_cast<double>(cb100w),
        static_cast<double>(cb102));
    reshade::log::message(reshade::log::level::info,line);
}

'''
def transform(src: str) -> str:
    src=src.replace('\r\n','\n')
    if MARKER in src:
        if src.count(MARKER)!=1 or src.count('v274_capture_actual_lerp_cb(')!=2:
            raise AssertionError('V274 partial source drift')
        return src
    if src.count(HEADER)!=1 or src.count(BEFORE)!=1 or src.count(AFTER)!=1:
        raise AssertionError('V272 draw runtime exact preimage missing')
    # Verify original exact material and stable family are untouched.
    if src.count('pmetal_envspec_receiver_family::\n            hemenvlerp') < 1:
        raise AssertionError('V272 Lerp family not found')
    changed=src.replace(HEADER,INSERT_HEADER,1)
    changed=changed.replace(BEFORE,BEFORE+HELPER,1)
    changed=changed.replace(AFTER,CALL,1)
    assert changed.count(MARKER)==1
    assert changed.count('v274_capture_actual_lerp_cb(')==2
    return changed

def main() -> None:
    p=argparse.ArgumentParser()
    p.add_argument('--apply',action='store_true')
    args=p.parse_args()
    orig=P.read_text(encoding='utf-8')
    updated=transform(orig)
    if args.apply:
        if updated!=orig: P.write_text(updated,encoding='utf-8',newline='\n')
    elif updated!=orig:
        raise AssertionError('V274 witness was not applied')
    print('V274_LERP_ONLY_ACTUAL_GPU_CB85_T1_T10_WITNESS_'+
          ('APPLIED' if args.apply else 'VERIFIED'))
    print('V274_STABLE_HEMENV_SHADER_CB_RESOURCE_EQ_UNCHANGED')
    print('V274_LERP_GPU_CB_READ_ONLY_MAX96_PER_UNIQUE_MATERIAL_BETA')
    print('V274_RUNTIME_AND_PIXEL=OPEN')
if __name__=='__main__':
    main()
