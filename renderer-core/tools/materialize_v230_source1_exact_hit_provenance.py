#!/usr/bin/env python3
"""Instrument verified exact producer consumer hits; leave fallback untouched.

Runs after materialize_v230_source1_repair.py and before native compiler.
Diagnostic scope: source_kind=1 provenance ONLY. No renderer operator or
LightBank selection/cache mutation.
"""
from pathlib import Path
p=Path(__file__).resolve().parents[1]/"src/runtime/pmetal_env_source_runtime.cpp"
s=p.read_text("utf-8")
before="""#if defined(DSRRL_V230_SOURCE1_REPAIR)
        g_source1_exact.fetch_add(1u,std::memory_order_relaxed);
#endif
        telemetry::hot_count("""
after="""#if defined(DSRRL_V230_SOURCE1_REPAIR)
        const auto exact_n=g_source1_exact.fetch_add(
            1u,std::memory_order_relaxed)+1u;
#if defined(DSRRL_V230_SOURCE1_EXACT_PROVENANCE)
        // Rate-limited by an atomic lifetime counter. Never read the owner
        // address or dereference a nonvalidated object in this observer.
        if (exact_n<=40u || exact_n==64u ||
            exact_n==128u || exact_n==256u) {
            char buf[530]{};
            std::snprintf(
                buf,sizeof(buf),
                "[DSRRL SOURCE1 HIT] n=%llu route=%u slot=%u "
                "flver8=%02x%02x%02x%02x mtd8=%02x%02x%02x%02x "
                "owner_exact=%u actual_exact=%u slot_valid=%u "
                "bank=%016llx row=%u beta=%.8g epoch=%llu "
                "tid=%lu",
                static_cast<unsigned long long>(exact_n),
                static_cast<unsigned>(material.route_index),
                static_cast<unsigned>(material.material_slot),
                material.flver_sha256[0],material.flver_sha256[1],
                material.flver_sha256[2],material.flver_sha256[3],
                material.raw_mtd_sha256[0],material.raw_mtd_sha256[1],
                material.raw_mtd_sha256[2],material.raw_mtd_sha256[3],
                material.owner_tuple_exact?1u:0u,
                material.actual_material_exact?1u:0u,
                material.material_slot_valid?1u:0u,
                static_cast<unsigned long long>(out.bank_signature_a),
                static_cast<unsigned>(out.row_id_a),
                static_cast<double>(out.beta),
                static_cast<unsigned long long>(epoch),
                static_cast<unsigned long>(GetCurrentThreadId()));
            reshade::log::message(
                reshade::log::level::warning,buf);
        }
#endif
#endif
        telemetry::hot_count("""
if s.count(before)!=1:
    raise RuntimeError("V230 source1 exact-hit diagnostic drift: "+str(s.count(before)))
s=s.replace(before,after,1)
p.write_text(s,"utf-8")
print("SOURCE1_EXACT_HIT_PROVENANCE_PASS no source/routing mutation")
