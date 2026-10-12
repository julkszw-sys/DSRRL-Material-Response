#!/usr/bin/env python3
"""V251 one-shot RX33/RX34 PMetal LERP native D3D11 state witness."""
from pathlib import Path
import argparse
H=Path('renderer-core/include/dsrrl/runtime/draw_state_transaction.hpp')
T=Path('renderer-core/src/runtime/draw_state_transaction.cpp')
I=Path('renderer-core/integrated/integrated_addon.cpp')
h0='    bool synchronous_core_transaction = false;\n};'
h1='    bool synchronous_core_transaction = false;\n    bool v251_lerp_witness = false;\n    std::uint32_t v251_rx = 0u;\n};'
t0='    bool bound = true;\n\n    if (state.verify_native_readback) {'
t1=r'''    // V251: read-only, first exact PMetal LERP native bind observation.
    if (mutation.v251_lerp_witness) {
        ID3D11PixelShader *ps = nullptr;
        ctx->PSGetShader(&ps, nullptr, nullptr);
        const bool ps_ok = ps == mutation.pixel_shader;
        if (ps != nullptr) ps->Release();
        bool cb_ok = true, srv_ok = true, smp_ok = true;
        std::uint32_t cb_bad = 0u, srv_bad = 0u, smp_bad = 0u;
        for (std::uint32_t i = 0; i < mutation.constant_buffer_count; ++i) {
            const auto &b = mutation.constant_buffers[i];
            ID3D11Buffer *v = nullptr;
            ctx->PSGetConstantBuffers(b.slot, 1u, &v);
            if (v != b.buffer) {
                cb_ok = false;
                if (b.slot < 32u) cb_bad |= (1u << b.slot);
            }
            if (v != nullptr) v->Release();
        }
        for (std::uint32_t i = 0; i < mutation.srv_count; ++i) {
            const auto &b = mutation.srvs[i];
            ID3D11ShaderResourceView *v = nullptr;
            ctx->PSGetShaderResources(b.slot, 1u, &v);
            if (v != b.srv) {
                srv_ok = false;
                if (b.slot < 32u) srv_bad |= (1u << b.slot);
            }
            if (v != nullptr) v->Release();
        }
        for (std::uint32_t i = 0; i < mutation.sampler_count; ++i) {
            const auto &b = mutation.samplers[i];
            ID3D11SamplerState *v = nullptr;
            ctx->PSGetSamplers(b.slot, 1u, &v);
            if (v != b.sampler) {
                smp_ok = false;
                if (b.slot < 32u) smp_bad |= (1u << b.slot);
            }
            if (v != nullptr) v->Release();
        }
        char msg[450]{};
        std::snprintf(msg,sizeof(msg),
            "[DSRRL V251 LERP NATIVE] rx=%u route=345 ctx=%s "
            "ps=%u cb=%u srv=%u smp=%u cb_bad=%08x "
            "srv_bad=%08x smp_bad=%08x cb_n=%u srv_n=%u smp_n=%u "
            "stage=BEFORE_DRAW mutation_unchanged=1 pixel=OPEN",
            mutation.v251_rx,
            ctx->GetType() == D3D11_DEVICE_CONTEXT_DEFERRED
                ? "DEFERRED" : "IMMEDIATE",
            ps_ok?1u:0u,cb_ok?1u:0u,srv_ok?1u:0u,smp_ok?1u:0u,
            cb_bad,srv_bad,smp_bad,mutation.constant_buffer_count,
            mutation.srv_count,mutation.sampler_count);
        reshade::log::message(reshade::log::level::info,msg);
    }

    bool bound = true;

    if (state.verify_native_readback) {'''
g0='std::atomic<std::uint64_t> g_lerp_full_draw_fallback{0};\n'
g1=g0+'std::atomic<std::uint32_t> g_v251_lerp_native_seen{0u};\n'
a0='    if (k_pmetal_native_draw_runtime_enabled &&\n        (prepared.envspec_in_batch ||'
a1='''    if (hemenvlerp_bound && prepared.envspec_in_batch &&
        decision.active && decision.route_index == 345u &&
        material.valid && material.owner_tuple_exact &&
        material.material_slot_valid &&
        (receiver_id == 33u || receiver_id == 34u)) {
        const auto bit = receiver_id == 33u ? 1u : 2u;
        const auto seen = g_v251_lerp_native_seen.load(
            std::memory_order_relaxed);
        if ((seen & bit) == 0u &&
            (g_v251_lerp_native_seen.fetch_or(
                bit,std::memory_order_relaxed) & bit) == 0u) {
            prepared.batch.mutation.v251_lerp_witness = true;
            prepared.batch.mutation.v251_rx = receiver_id;
        }
    }
''' + a0
def edit(p,old,new,num,apply):
    s=p.read_text(encoding='utf8').replace('\r\n','\n')
    if s.count(new)==num:return
    if s.count(old)!=num:raise RuntimeError(f'V251 anchor mismatch {p} {s.count(old)} expected {num}')
    if apply:p.write_text(s.replace(old,new,num),encoding='utf8',newline='\n')
    else:raise RuntimeError(f'V251 not applied {p}')
if __name__=='__main__':
    a=argparse.ArgumentParser();a.add_argument('--apply',action='store_true');v=a.parse_args()
    for p,o,n,k in [(H,h0,h1,1),(T,t0,t1,1),(I,g0,g1,1),(I,a0,a1,2)]:
        edit(p,o,n,k,v.apply)
    print('V251_EXACT_RX33_34_PMETAL_LERP_NATIVE_READBACK_'+('APPLIED' if v.apply else 'VERIFIED'))
