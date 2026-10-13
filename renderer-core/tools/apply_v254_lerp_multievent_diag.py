#!/usr/bin/env python3
"""V254: read-only multi-event HemEnvLerp observer on generated V253 sources.

Run strictly AFTER apply_v253_ptde_envspec_rgbm.py --apply and the V252/V250
source overlays. No shader, source, descriptor, CB, SRV, draw or routing mutation.
All new observers are diagnostic. Existing V253 runtime behavior is unchanged.
"""
from pathlib import Path
import argparse

INTEGRATED = Path('renderer-core/integrated/integrated_addon.cpp')
PMETAL = Path('renderer-core/src/runtime/pmetal_envspec_draw_runtime.cpp')

DECL_ANCHOR = 'void v250_pmetal_lerp_draw_proof(\n'
DECL_REPLACEMENT = '''// V254 observer declaration, implemented before on_draw.
void v254_lerp_event(unsigned cls, unsigned rx, unsigned stage, unsigned route,
    unsigned slot, std::uint64_t mtd, std::uint64_t flver,
    unsigned tx, float c100x, float c100y, float c100z,
    float c101, float specpow, unsigned specpow_valid) noexcept;

''' + DECL_ANCHOR

GLOBAL_ANCHOR = 'void v252_nonmetal_lerp_stage(\n'
GLOBAL_REPLACEMENT = r'''// V254: shared, read-only, multi-event LERP diagnostic.
// Unlike V250/V252 first-hit latches this counts every eligible transition.
// Logs first 8, dense early samples and sparse late samples per class/receiver/stage.
// At present multiples of 600, counters report even nonlogged events.
// The hot path adds one relaxed atomic per eligible event and no GPU reads.
static std::atomic<std::uint64_t> g_v254_lerp_events[4][3][5]{};
static std::atomic<std::uint64_t> g_v254_lerp_emitted[4][3][5]{};

void v254_lerp_event(
    unsigned cls, unsigned rx, unsigned stage, unsigned route,
    unsigned slot, std::uint64_t mtd, std::uint64_t flver,
    unsigned tx, float c100x, float c100y, float c100z,
    float c101, float specpow, unsigned specpow_valid) noexcept
{
    if (cls >= 4u || rx < 33u || rx > 35u || stage >= 5u)
        return;
    const auto n = g_v254_lerp_events[cls][rx - 33u][stage].fetch_add(
        1u, std::memory_order_relaxed) + 1u;
    if (!(n <= 8u ||
          (n <= 4096u && (n % 64u) == 0u) ||
          (n <= 524288u && (n % 2048u) == 0u) ||
          ((n % 131072u) == 0u)))
        return;
    g_v254_lerp_emitted[cls][rx - 33u][stage].fetch_add(
        1u, std::memory_order_relaxed);
    char line[620]{};
    std::snprintf(line, sizeof(line),
        "[DSRRL V254 LERP EVENTS] seq=%llu class=%u rx=%u stage=%u "
        "route=%u slot=%u mtd=%016llx flver=%016llx tx=%u "
        "c100=%.7g,%.7g,%.7g c101=%.7g specpow=%.7g sp_valid=%u "
        "phn_merge=NOT_GPU_CAPTURED pixel=OPEN",
        static_cast<unsigned long long>(n), cls, rx, stage,
        route, slot, static_cast<unsigned long long>(mtd),
        static_cast<unsigned long long>(flver), tx,
        static_cast<double>(c100x), static_cast<double>(c100y),
        static_cast<double>(c100z), static_cast<double>(c101),
        static_cast<double>(specpow), specpow_valid);
    reshade::log::message(reshade::log::level::info, line);
}

void v254_lerp_summary(std::uint64_t present) noexcept
{
    if (present == 0u || (present % 600u) != 0u)
        return;
    for (unsigned cls = 0u; cls < 4u; ++cls) {
        std::uint64_t totals[5]{}, emitted[5]{};
        for (unsigned rx = 0u; rx < 3u; ++rx)
            for (unsigned stage = 0u; stage < 5u; ++stage) {
                totals[stage] += g_v254_lerp_events[cls][rx][stage].load(
                    std::memory_order_relaxed);
                emitted[stage] += g_v254_lerp_emitted[cls][rx][stage].load(
                    std::memory_order_relaxed);
            }
        if (totals[0] + totals[1] + totals[2] + totals[3] + totals[4] == 0u)
            continue;
        char line[480]{};
        std::snprintf(line, sizeof(line),
            "[DSRRL V254 LERP SUMMARY] present=%llu class=%u "
            "events_stage_0_4=%llu,%llu,%llu,%llu,%llu "
            "logged_stage_0_4=%llu,%llu,%llu,%llu,%llu "
            "suppressed=events_minus_logged single_hit=NO",
            static_cast<unsigned long long>(present), cls,
            static_cast<unsigned long long>(totals[0]),
            static_cast<unsigned long long>(totals[1]),
            static_cast<unsigned long long>(totals[2]),
            static_cast<unsigned long long>(totals[3]),
            static_cast<unsigned long long>(totals[4]),
            static_cast<unsigned long long>(emitted[0]),
            static_cast<unsigned long long>(emitted[1]),
            static_cast<unsigned long long>(emitted[2]),
            static_cast<unsigned long long>(emitted[3]),
            static_cast<unsigned long long>(emitted[4]));
        reshade::log::message(reshade::log::level::info, line);
    }
}

''' + GLOBAL_ANCHOR

NONMETAL_ANCHOR = '''    const unsigned index =
        (static_cast<unsigned>(route_number) * 2u +'''
NONMETAL_REPLACEMENT = r'''    // V254 stage: 0 prepare-fail; 1 batch-ready; 2 native-armed;
    // 3 actually-issued; 4 dispatch-fail. Do not infer pixel quality.
    v254_lerp_event(
        static_cast<unsigned>(route_number) + 1u,
        receiver, stage, static_cast<unsigned>(decision.route_index),
        static_cast<unsigned>(material.material_slot),
        material.semantic_name_hash, material.flver_identity_hash, tx,
        decision.c100[0], decision.c100[1], decision.c100[2],
        decision.c101, decision.ptde_specular_power,
        decision.ptde_specular_power_verified ? 1u : 0u);

''' + NONMETAL_ANCHOR

PMETAL_ANCHOR = '''    const unsigned cls = route == 345u ? 0u : 1u;
'''
PMETAL_REPLACEMENT = r'''    // V254 exact route345 stage mapping. No MTD/CB donor is claimed
    // from this lightweight v250 witness; source event logs those separately.
    v254_lerp_event(0u, rx,
        stage == 2u ? 3u : stage == 3u ? 4u : stage,
        route, 0u, 0u, 0u, tx,
        0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0u);
''' + PMETAL_ANCHOR

PRESENT_ANCHOR = '''    g_clustered_pnts.frame_event(present);
'''
PRESENT_REPLACEMENT = PRESENT_ANCHOR + '''    v254_lerp_summary(present);
'''

SOURCE_ANCHOR = '''    std::array<float,3> carrier3{{
        ptde_spec_b[0],'''
SOURCE_REPLACEMENT = r'''    // V254 separate source hypothesis: genuine A/B donor and host values,
    // EnvDiffuse endpoints, effective shared beta, material c101 and PHN k135.
    // The original PTDE c84.w/c85.w/c110.w are NOT read here; never equate
    // this witness with PTDE producer equality or actual GPU pixel values.
    if (family == pmetal_envspec_receiver_family::hemenvlerp) {
        static std::atomic<std::uint64_t> seen[3][8]{};
        const unsigned rx = static_cast<unsigned>(decision.receiver_id);
        if (rx >= 33u && rx <= 35u) {
            const float beta = source.beta;
            const unsigned band = !std::isfinite(beta) ? 7u :
                beta <= 0.0f ? 0u : beta >= 1.0f ? 7u :
                static_cast<unsigned>(beta * 8.0f);
            const auto n = seen[rx - 33u][band].fetch_add(
                1u, std::memory_order_relaxed) + 1u;
            if (n <= 8u ||
                (n <= 4096u && (n % 64u) == 0u) ||
                (n <= 524288u && (n % 2048u) == 0u) ||
                (n % 131072u) == 0u) {
                const float mixs0 = ptde_spec_a[0] +
                    beta * (ptde_spec_b[0] - ptde_spec_a[0]);
                const float mixd0 = source.envdiffuse_a[0] +
                    beta * (source.envdiffuse_b[0] - source.envdiffuse_a[0]);
                char line[1280]{};
                std::snprintf(line,sizeof(line),
                    "[DSRRL V254 LERP SOURCE] seq=%llu rx=%u route=%u "
                    "beta_band=%u host_beta=%.9g "
                    "A=%016llx/%u B=%016llx/%u "
                    "dsr_spcA=%.7g,%.7g,%.7g dsr_spcB=%.7g,%.7g,%.7g "
                    "ptde_spcA=%.7g,%.7g,%.7g ptde_spcB=%.7g,%.7g,%.7g "
                    "ptde_envdiffA=%.7g,%.7g,%.7g ptde_envdiffB=%.7g,%.7g,%.7g "
                    "blend_spc_r=%.7g blend_diff_r=%.7g "
                    "c100=%.7g c101=%.7g phn_k135_valid=%u phn_k135=%.7g "
                    "ptde_c84w=UNOBSERVED ptde_c85w=UNOBSERVED "
                    "ptde_c110w=UNOBSERVED merge_pixels=UNOBSERVED",
                    static_cast<unsigned long long>(n),rx,
                    static_cast<unsigned>(decision.route_index), band,
                    static_cast<double>(beta),
                    static_cast<unsigned long long>(source.bank_signature_a),
                    static_cast<unsigned>(source.row_id_a),
                    static_cast<unsigned long long>(source.bank_signature_b),
                    static_cast<unsigned>(source.row_id_b),
                    static_cast<double>(source.a[0]),
                    static_cast<double>(source.a[1]),
                    static_cast<double>(source.a[2]),
                    static_cast<double>(source.b[0]),
                    static_cast<double>(source.b[1]),
                    static_cast<double>(source.b[2]),
                    static_cast<double>(ptde_spec_a[0]),
                    static_cast<double>(ptde_spec_a[1]),
                    static_cast<double>(ptde_spec_a[2]),
                    static_cast<double>(ptde_spec_b[0]),
                    static_cast<double>(ptde_spec_b[1]),
                    static_cast<double>(ptde_spec_b[2]),
                    static_cast<double>(source.envdiffuse_a[0]),
                    static_cast<double>(source.envdiffuse_a[1]),
                    static_cast<double>(source.envdiffuse_a[2]),
                    static_cast<double>(source.envdiffuse_b[0]),
                    static_cast<double>(source.envdiffuse_b[1]),
                    static_cast<double>(source.envdiffuse_b[2]),
                    static_cast<double>(mixs0),
                    static_cast<double>(mixd0),
                    static_cast<double>(decision.c100[0]),
                    static_cast<double>(decision.c101),
                    source.phn_k135_valid ? 1u : 0u,
                    static_cast<double>(source.phn_k135));
                reshade::log::message(reshade::log::level::info,line);
            }
        }
    }

''' + SOURCE_ANCHOR

# V251's previous native PS/CB/SRV/sampler witness was a first-hit bitmask.
# Re-arm that exact readback only on sampled, valid, PREPARED P_Metal draws;
# no new D3D queries on nonselected draws and no new transaction semantics.
NATIVE_ANCHOR = '''    if (k_pmetal_native_draw_runtime_enabled &&
        (prepared.envspec_in_batch ||'''
NATIVE_REPLACEMENT = r'''    if (hemenvlerp_bound && prepared.envspec_in_batch &&
        decision.active && decision.route_index == 345u &&
        material.valid && material.owner_tuple_exact &&
        material.material_slot_valid &&
        (receiver_id == 33u || receiver_id == 34u)) {
        static std::atomic<std::uint64_t> sampled[2]{};
        const unsigned idx = receiver_id - 33u;
        const auto n = sampled[idx].fetch_add(1u,
            std::memory_order_relaxed) + 1u;
        if (n <= 8u ||
            (n <= 4096u && (n % 64u) == 0u) ||
            (n <= 524288u && (n % 2048u) == 0u) ||
            (n % 131072u) == 0u) {
            // Read-only existing V251 PRE-DRAW native object equality.
            prepared.batch.mutation.v251_lerp_witness = true;
        }
    }
''' + NATIVE_ANCHOR


def change(data, old, new, label):
    if data.count(new) == 1:
        return data
    count = data.count(old)
    if count != 1:
        raise ValueError(f'V254 {label}: expected one preimage, got {count}')
    return data.replace(old, new, 1)


def patch_integrated(s):
    # Enforce that V250 and V252 observer materializers already ran.
    for token in ('[DSRRL V250 PMETAL LERP DRAW]',
                  '[DSRRL V252 NONMETAL LERP]'):
        if token not in s:
            raise ValueError('V254 requires previous observer: '+token)
    s = change(s, DECL_ANCHOR, DECL_REPLACEMENT, 'forward-decl')
    s = change(s, GLOBAL_ANCHOR, GLOBAL_REPLACEMENT, 'globals')
    s = change(s, NONMETAL_ANCHOR, NONMETAL_REPLACEMENT, 'nonmetal')
    s = change(s, PMETAL_ANCHOR, PMETAL_REPLACEMENT, 'pmetal')
    s = change(s, PRESENT_ANCHOR, PRESENT_REPLACEMENT, 'present')
    if s.count(NATIVE_REPLACEMENT) not in (0,2):
        raise ValueError('V254 native sampler inconsistent')
    if s.count(NATIVE_REPLACEMENT) == 0:
        if s.count(NATIVE_ANCHOR) != 2:
            raise ValueError('V254 native call sites not exactly two')
        s = s.replace(NATIVE_ANCHOR, NATIVE_REPLACEMENT, 2)
    return s


def patch_pmetal(s):
    if '[DSRRL V253 PTDE ENVSPEC RGBM]' not in s:
        raise ValueError('V254 needs exact V253 RGBM source')
    return change(s, SOURCE_ANCHOR, SOURCE_REPLACEMENT, 'pmetal-source')


def main():
    p=argparse.ArgumentParser()
    p.add_argument('--apply', action='store_true')
    args=p.parse_args()
    original_integrated=INTEGRATED.read_text(encoding='utf8').replace('\r\n','\n')
    original_source=PMETAL.read_text(encoding='utf8').replace('\r\n','\n')
    new_integrated=patch_integrated(original_integrated)
    new_source=patch_pmetal(original_source)
    if not args.apply and (original_integrated!=new_integrated or original_source!=new_source):
        raise RuntimeError('V254 overlay is not applied')
    if args.apply:
        if original_integrated!=new_integrated:
            INTEGRATED.write_text(new_integrated,encoding='utf8',newline='\n')
        if original_source!=new_source:
            PMETAL.write_text(new_source,encoding='utf8',newline='\n')
    print('V254_LERP_MULTIEVENT_NONMUTATING_ORACLE_'+('APPLIED' if args.apply else 'PASS'))

if __name__=='__main__':
    main()
