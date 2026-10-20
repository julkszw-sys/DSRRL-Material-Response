#!/usr/bin/env python3
"""V351 opt-in post-materializer: DSR-only typed sources remain on the
modified EnvSpec+EnvDiffuse+material operator, never switch its draw to stock
because the donor is tagged DSR_ONLY. A failed source, unverified MTD/receiver,
shader/resource transaction still fails open and BLOCKS release.
Must run AFTER V304 typed materializer.
"""
from pathlib import Path

root=Path(__file__).resolve().parents[1]
owner=root/"src/runtime/material_owner_selection.cpp"
draw=root/"src/runtime/pmetal_envspec_draw_runtime.cpp"

o=owner.read_text(encoding="utf-8")
anchor='#include "dsrrl/runtime/material_owner_selection.hpp"'
assert o.count(anchor)==1, "owner source drift"
o=o.replace(anchor,anchor+'\n#include "dsrrl/runtime/v351_exact_material_route_recovery.hpp"',1)
before='''    const bool exact_runtime_material =
        operators::material_response::
            exact_runtime_material_response_identity(
                identity);

    const bool flver_shape_valid =
        identity.valid &&
        identity.owner_tuple_exact &&
        identity.material_slot_valid &&
        identity.semantic_name_hash != 0u;'''
after='''#if defined(DSRRL_V351_EXACT_MATERIAL_ROUTE_RECOVERY)
    const auto resolved =
        v351::recover_exact_route(identity);
#else
    const auto &resolved = identity;
#endif
    const bool exact_runtime_material =
        operators::material_response::
            exact_runtime_material_response_identity(
                resolved);

    const bool flver_shape_valid =
        resolved.valid &&
        resolved.owner_tuple_exact &&
        resolved.material_slot_valid &&
        resolved.semantic_name_hash != 0u;'''
assert o.count(before)==1, "owner publication anchor changed"
o=o.replace(before,after,1)
tail=o.split('bool material_owner_selection_publish(',1)
assert len(tail)==2
head,body=tail
body_part,rest=body.split('\nbool material_owner_selection_consume(',1)
assert body_part.count('owner_tuple_authenticated_cached(\n            identity)')==1
assert body_part.count('g_current = identity;')==1
body_part=body_part.replace('owner_tuple_authenticated_cached(\n            identity)','owner_tuple_authenticated_cached(\n            resolved)',1).replace('g_current = identity;','g_current = resolved;',1)
o=head+'bool material_owner_selection_publish('+body_part+'\nbool material_owner_selection_consume('+rest
owner.write_text(o,encoding="utf-8")

s=draw.read_text(encoding="utf-8")
stock='''    if (dsr_only_draw) {
        telemetry::hot_count(source_rejects_);
        effect_fail(effect_fail_mask_,k_effect_fail_source);
        log_prepare_stage_once(
            1u << 13u,
            "v304_typed_dsr_only_native_draw",
            material,decision,family);
        return false;
    }
#endif
    effect_latch(effect_receiver_source_ready_);'''
modified='''#if defined(DSRRL_V351_TYPED_DSR_ONLY_OPERATOR)
    if (dsr_only_draw) {
        // Source_.latest() already verified an exact material/receiver producer.
        // V304's independent EnvSpec and EnvDiffuse typed decoders return
        // actual DSR values, not PTDE row63 aliases. Feed those values into
        // the existing MODIFIED PTDE shader/CB/sidecar composition.
        // The provenance type is not an operator-disable decision.
        const bool typed_source_valid =
#if defined(DSRRL_PMETAL_STOCK_DSR_ENVDIFFUSE)
            // EnvSpec still needs the exact PTDE endpoints and beta, but the
            // stock-EnvDiffuse variant must not depend on EnvDiffuse donors.
            std::isfinite(source.a[0]) && std::isfinite(source.a[1]) &&
            std::isfinite(source.a[2]) && std::isfinite(source.b[0]) &&
            std::isfinite(source.b[1]) && std::isfinite(source.b[2]);
#else
            source.envdiffuse_linear_valid &&
            std::isfinite(source.a[0]) && std::isfinite(source.a[1]) &&
            std::isfinite(source.a[2]) && std::isfinite(source.b[0]) &&
            std::isfinite(source.b[1]) && std::isfinite(source.b[2]) &&
            std::isfinite(source.envdiffuse_a[0]) &&
            std::isfinite(source.envdiffuse_a[1]) &&
            std::isfinite(source.envdiffuse_a[2]) &&
            std::isfinite(source.envdiffuse_b[0]) &&
            std::isfinite(source.envdiffuse_b[1]) &&
            std::isfinite(source.envdiffuse_b[2]);
#endif
        if (!typed_source_valid) {
            telemetry::hot_count(source_rejects_);
            effect_fail(effect_fail_mask_,k_effect_fail_source);
            log_prepare_stage_once(
                1u << 13u,"v351_typed_source_invalid_RELEASE_BLOCK",
                material,decision,family);
            return false;
        }

        static std::atomic<std::uint32_t> logged{0u};
        std::uint32_t mask = 0u;
        for (std::uint32_t i=0u; i<v300::k_dsr_only.size(); ++i) {
            const auto &dsr=v300::k_dsr_only[i];
            if ((source.bank_signature_a==dsr.signature &&
                 source.row_id_a==dsr.row_id) ||
                (source.bank_signature_b==dsr.signature &&
                 source.row_id_b==dsr.row_id))
                mask |= (1u << i);
        }
        if ((logged.fetch_or(mask,std::memory_order_relaxed)&mask)!=mask) {
            char message[400]{};
            std::snprintf(message,sizeof(message),
                "[DSRRL V351 DSR_ONLY_OPERATOR] stage=typed_source_into_modified_renderer"
                " family=%u rx=%u route=%u bank_a=%016llx row_a=%u"
                " bank_b=%016llx row_b=%u beta=%.9g"
                " envspec_a=%.9g,%.9g,%.9g"
                " envdiffuse_a=%.9g,%.9g,%.9g"
                " material_sidecars=KEEP_DRAW_ATOMIC pixel=OPEN",
                static_cast<unsigned>(family),
                decision.receiver_id,decision.route_index,
                static_cast<unsigned long long>(source.bank_signature_a),
                source.row_id_a,
                static_cast<unsigned long long>(source.bank_signature_b),
                source.row_id_b,static_cast<double>(source.beta),
                static_cast<double>(source.a[0]),
                static_cast<double>(source.a[1]),
                static_cast<double>(source.a[2]),
                static_cast<double>(source.envdiffuse_a[0]),
                static_cast<double>(source.envdiffuse_a[1]),
                static_cast<double>(source.envdiffuse_a[2]));
            reshade::log::message(reshade::log::level::info,message);
        }
    }
#else
    // Historical V304 native fallback, deliberately not part of V351 build.
    if (dsr_only_draw) {
        telemetry::hot_count(source_rejects_);
        effect_fail(effect_fail_mask_,k_effect_fail_source);
        log_prepare_stage_once(
            1u << 13u,
            "v304_typed_dsr_only_native_draw",
            material,decision,family);
        return false;
    }
#endif
#endif
    effect_latch(effect_receiver_source_ready_);'''
assert s.count(stock)==1, "V304 post-materializer guard changed"
s=s.replace(stock,modified,1)
draw.write_text(s,encoding="utf-8")
print("V351_TYPED_DSR_ONLY_MODIFIED_RENDERER_PATCH_PASS; WIP NOT RELEASE")
