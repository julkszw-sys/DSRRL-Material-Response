#!/usr/bin/env python3
"""V255: true P_Metal draw identity, RX native label, PHN carrier cut telemetry.

Generated-source overlay to run ONLY after exact V253 + V254 overlays.
NO actual equation, shader, resource, CB contents, draw, or restore mutation.
PTDE c110.w, c84.w, c85.w and true pre/post GPU pixel still UNOBSERVED.
"""
from pathlib import Path
import argparse

INTEGRATED=Path('renderer-core/integrated/integrated_addon.cpp')
PMETAL=Path('renderer-core/src/runtime/pmetal_envspec_draw_runtime.cpp')

# V254's P_Metal class 0 event logs used fake 0 metadata from the generic V250
# witness. Remove those *fake* events; replace them by exact authenticated
# material identities at prepared/actual-issue, NOT guessed source values.
FAKE_EVENT=r'''    // V254 exact route345 stage mapping. No MTD/CB donor is claimed
    // from this lightweight v250 witness; source event logs those separately.
    v254_lerp_event(0u, rx,
        stage == 2u ? 3u : stage == 3u ? 4u : stage,
        route, 0u, 0u, 0u, tx,
        0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0u);
'''
FAKE_REPLACEMENT='''    // V255: V250 retains its legacy first-hit proof only.
    // Full P_Metal V254 events are now emitted from authenticated draw scope.
'''

# V251 first-readback witness includes rx from the mutation. V254's repeated
# rearm updated the witness bit but omitted rx: observed 47/48 mislabeled 0.
RX_OLD='''            prepared.batch.mutation.v251_lerp_witness = true;
        }
    }
    if (k_pmetal_native_draw_runtime_enabled &&'''
RX_NEW='''            prepared.batch.mutation.v251_lerp_witness = true;
            prepared.batch.mutation.v251_rx = receiver_id;
        }
    }
    if (k_pmetal_native_draw_runtime_enabled &&'''

READY_ANCHOR='''    const auto effect_prepared =
        prepared_effect_mask(prepared);
'''
READY_ADD=r'''    // V255 authoritative P_Metal event: no placeholder zeros or duplicate
    // stage counts. Material is validated before the batch is prepared.
    if (hemenvlerp_bound && decision.active &&
        decision.route_index == 345u &&
        (receiver_id == 33u || receiver_id == 34u) &&
        material.valid && material.owner_tuple_exact &&
        material.material_slot_valid) {
        v254_lerp_event(0u, receiver_id, 1u,
            static_cast<unsigned>(decision.route_index),
            static_cast<unsigned>(material.material_slot),
            material.semantic_name_hash,
            material.flver_identity_hash, 0u,
            decision.c100[0], decision.c100[1], decision.c100[2],
            decision.c101, decision.ptde_specular_power,
            decision.ptde_specular_power_verified ? 1u : 0u);
    }
''' + READY_ANCHOR

ISSUE_ANCHOR='''    account_effect_dispatch(
        effect_prepared,
        dispatch.transaction,
        receiver_id,
        decision.route_index);
'''
ISSUE_ADD=ISSUE_ANCHOR+r'''    if (hemenvlerp_bound && decision.active &&
        decision.route_index == 345u &&
        (receiver_id == 33u || receiver_id == 34u) &&
        material.valid && material.owner_tuple_exact &&
        material.material_slot_valid) {
        v254_lerp_event(0u, receiver_id,
            dsrrl::runtime::draw_tx_issued(dispatch.transaction) ? 3u : 4u,
            static_cast<unsigned>(decision.route_index),
            static_cast<unsigned>(material.material_slot),
            material.semantic_name_hash,
            material.flver_identity_hash,
            static_cast<unsigned>(dispatch.transaction),
            decision.c100[0], decision.c100[1], decision.c100[2],
            decision.c101, decision.ptde_specular_power,
            decision.ptde_specular_power_verified ? 1u : 0u);
    }
'''

# Insert after the V254 source event, before b12 payload is constructed.
# CPU expected transport math is not a readback of GPU CB contents/pixels.
CUT_ANCHOR='''    std::array<float,3> carrier3{{
        ptde_spec_b[0],'''
CUT_ADD=r'''    // V255 strictly operator-local CPU transport witness. The values below
    // are the precise authored b12 payload candidates, not the c110 producer
    // and not a GPU snapshot of the final PHN shader result.
    if (family == pmetal_envspec_receiver_family::hemenvlerp &&
        (decision.receiver_id == 33u || decision.receiver_id == 34u)) {
        const auto rx = static_cast<unsigned>(decision.receiver_id);
        static std::atomic<std::uint64_t> samples[2]{};
        const auto n = samples[rx - 33u].fetch_add(
            1u, std::memory_order_relaxed) + 1u;
        if (n <= 12u ||
            (n <= 8192u && (n % 128u) == 0u) ||
            (n <= 1048576u && (n % 4096u) == 0u) ||
            (n % 262144u) == 0u) {
            const auto b = source.beta;
            const auto k135 = source.phn_k135_valid &&
                std::isfinite(source.phn_k135) && source.phn_k135 > 0.0f
                ? source.phn_k135 : 1.0f;
            const float spec0 = ptde_spec_a[0] +
                b * (ptde_spec_b[0] - ptde_spec_a[0]);
            const float spec1 = ptde_spec_a[1] +
                b * (ptde_spec_b[1] - ptde_spec_a[1]);
            const float spec2 = ptde_spec_a[2] +
                b * (ptde_spec_b[2] - ptde_spec_a[2]);
            const float diff0 = source.envdiffuse_a[0] +
                b * (source.envdiffuse_b[0] - source.envdiffuse_a[0]);
            const float diff1 = source.envdiffuse_a[1] +
                b * (source.envdiffuse_b[1] - source.envdiffuse_a[1]);
            const float diff2 = source.envdiffuse_a[2] +
                b * (source.envdiffuse_b[2] - source.envdiffuse_a[2]);
            const float spec_max_difference = std::fmax(
                std::fmax(std::fabs(ptde_spec_b[0] - ptde_spec_a[0]),
                          std::fabs(ptde_spec_b[1] - ptde_spec_a[1])),
                          std::fabs(ptde_spec_b[2] - ptde_spec_a[2]));
            const float diffuse_max_difference = std::fmax(
                std::fmax(std::fabs(source.envdiffuse_b[0] - source.envdiffuse_a[0]),
                          std::fabs(source.envdiffuse_b[1] - source.envdiffuse_a[1])),
                          std::fabs(source.envdiffuse_b[2] - source.envdiffuse_a[2]));
            char line[960]{};
            std::snprintf(line,sizeof(line),
                "[DSRRL V255 PHN CARRIER CUT] seq=%llu rx=%u route=%u "
                "bankA=%016llx/%u bankB=%016llx/%u beta=%.9g "
                "spc_interp_rgb=%.8g,%.8g,%.8g diff_interp_rgb=%.8g,%.8g,%.8g "
                "spec_endpoint_max_delta=%.8g diff_endpoint_max_delta=%.8g "
                "b12w0_expected=%.8g b12w3_expected=%.9g b12w5_expected=%.9g "
                "k135_producer_valid=%u k135_fallback_unity=%u "
                "c101=%.7g c100=%.7g,%.7g,%.7g "
                "PTDE_c84w=UNOBSERVED PTDE_c85w=UNOBSERVED PTDE_c110w=UNOBSERVED "
                "pre_post_PHN_RGB=NOT_CAPTURED pixel=OPEN",
                static_cast<unsigned long long>(n), rx,
                static_cast<unsigned>(decision.route_index),
                static_cast<unsigned long long>(source.bank_signature_a),
                static_cast<unsigned>(source.row_id_a),
                static_cast<unsigned long long>(source.bank_signature_b),
                static_cast<unsigned>(source.row_id_b),
                static_cast<double>(b), static_cast<double>(spec0),
                static_cast<double>(spec1),static_cast<double>(spec2),
                static_cast<double>(diff0),static_cast<double>(diff1),
                static_cast<double>(diff2),
                static_cast<double>(spec_max_difference),
                static_cast<double>(diffuse_max_difference),
                static_cast<double>(k135),static_cast<double>(b),
                static_cast<double>(b),
                source.phn_k135_valid ? 1u : 0u,
                source.phn_k135_valid ? 0u : 1u,
                static_cast<double>(decision.c101),
                static_cast<double>(decision.c100[0]),
                static_cast<double>(decision.c100[1]),
                static_cast<double>(decision.c100[2]));
            reshade::log::message(reshade::log::level::info,line);
        }
    }

''' + CUT_ANCHOR

def one(src,old,new,label,n=1):
    # Distinguish a pristine/mixed state. Do not silently reapply half patch.
    # New body may contain the old anchor as its suffix/prefix.
    # Therefore check the fully patched preimage FIRST (V254 contract).
    if src.count(new)==n:
        return src
    count=src.count(old)
    if count==n:
        return src.replace(old,new,n)
    raise RuntimeError(f'V255 {label}: unexpected preimage={count}, output={src.count(new)}')

def patch_integrated(src):
    if 'V254_LERP' not in src and '[DSRRL V254 LERP EVENTS]' not in src:
        raise RuntimeError('V255 requires applied V254 overlay')
    src=one(src,FAKE_EVENT,FAKE_REPLACEMENT,'remove false P_Metal placeholders')
    src=one(src,RX_OLD,RX_NEW,'V251 RX label fix',2)
    src=one(src,READY_ANCHOR,READY_ADD,'PMetal exact batch-ready',2)
    src=one(src,ISSUE_ANCHOR,ISSUE_ADD,'PMetal actual issue',2)
    return src

def patch_pmetal(src):
    if '[DSRRL V254 LERP SOURCE]' not in src:
        raise RuntimeError('V255 requires V254 source observer')
    return one(src,CUT_ANCHOR,CUT_ADD,'PHN expected-carrier operator cut')

def main():
    args=argparse.ArgumentParser()
    args.add_argument('--apply',action='store_true')
    opts=args.parse_args()
    old_i=INTEGRATED.read_text(encoding='utf8').replace('\r\n','\n')
    old_p=PMETAL.read_text(encoding='utf8').replace('\r\n','\n')
    new_i=patch_integrated(old_i)
    new_p=patch_pmetal(old_p)
    if opts.apply:
        if old_i!=new_i:INTEGRATED.write_text(new_i,encoding='utf8',newline='\n')
        if old_p!=new_p:PMETAL.write_text(new_p,encoding='utf8',newline='\n')
    elif old_i!=new_i or old_p!=new_p:
        raise RuntimeError('V255 source overlay not applied')
    print('V255_LERP_PHN_CARRIER_CUT_'+('APPLIED' if opts.apply else 'PASS'))

if __name__=='__main__':main()
