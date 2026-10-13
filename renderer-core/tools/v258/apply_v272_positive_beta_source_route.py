#!/usr/bin/env python3
"""V272: owner-limited real RX LERP b12 input provenance, beta>0 ONLY.

One-addon source overlay applied after V271. This is *runtime* evidence
instrumentation, not an inferred PTDE c84.w/c85.w producer. It never rewrites
the payload/PS/resource or creates draw replay. Capped to one observation per
interior beta band so the v249 beta=0-only selection cannot recur.
"""
import argparse
from pathlib import Path
P=Path("renderer-core/src/runtime/pmetal_envspec_draw_runtime.cpp")
A='''    prepared.request.constant_buffer_count =
        1u;'''
M='[DSRRL V272 INTERIOR LERP B12 ROUTE]'
B=r'''    prepared.request.constant_buffer_count =
        1u;

    // V272: producer -> already-packed b12 -> native DRAW REQUEST boundary.
    // A distinct PTDE c84.w/c85.w has not been measured. These two fields
    // currently share the *DSR* source.beta: never label either PTDE-valid.
    if (family ==
            pmetal_envspec_receiver_family::hemenvlerp &&
        material.valid && material.owner_tuple_exact &&
        decision.active &&
        std::isfinite(source.beta) &&
        source.beta > 0.0f && source.beta < 1.0f &&
        prepared.request.constant_buffers[0].slot == 12u &&
        prepared.request.constant_buffers[0].buffer == b12) {
#if defined(DSRRL_PMETAL_R19_LERP_EXACT_ENVDIFFUSE)
        const unsigned raw_band =
            static_cast<unsigned>(source.beta * 8.0f);
        const unsigned band = raw_band < 8u ? raw_band : 7u;
        static std::atomic<std::uint32_t> v272_seen_interior{0u};
        const std::uint32_t bit = 1u << band;
        if (!(v272_seen_interior.fetch_or(
                  bit,std::memory_order_relaxed) & bit)) {
            char evidence[840]{};
            std::snprintf(
                evidence,sizeof(evidence),
                "[DSRRL V272 INTERIOR LERP B12 ROUTE] "
                "phase=PREPARED_NOT_DRAW_ISSUED rx=%u route=%u "
                "owner=%016llx slot=%u band=%u "
                "dsr_source_beta=%.9g b12_spec_w3=%.9g b12_diffuse_w5=%.9g "
                "source_serial=%llu source_generation=%llu "
                "bank_a=%016llx row_a=%u bank_b=%016llx row_b=%u "
                "envdiffuse_valid=%u ptde_c84w=UNOBSERVED "
                "ptde_c85w=UNOBSERVED b12_resource_match=1 "
                "gpu_cb_readback=NOT_CAPTURED pixel=OPEN",
                static_cast<unsigned>(decision.receiver_id),
                static_cast<unsigned>(decision.route_index),
                static_cast<unsigned long long>(
                    material.flver_identity_hash),
                static_cast<unsigned>(material.material_slot),
                band,
                static_cast<double>(source.beta),
                static_cast<double>(payload[3][3]),
                static_cast<double>(payload[5][3]),
                static_cast<unsigned long long>(source.serial),
                static_cast<unsigned long long>(source.generation),
                static_cast<unsigned long long>(source.bank_signature_a),
                static_cast<unsigned>(source.row_id_a),
                static_cast<unsigned long long>(source.bank_signature_b),
                static_cast<unsigned>(source.row_id_b),
                source.envdiffuse_linear_valid ? 1u : 0u);
            reshade::log::message(reshade::log::level::info,evidence);
        }
#endif
    }'''
def main():
 p=argparse.ArgumentParser();p.add_argument('--apply',action='store_true');a=p.parse_args()
 s=P.read_text(encoding="utf8").replace("\r\n","\n")
 if M in s:
  if s.count(M)!=1 or s.count('v272_seen_interior')!=1:
   raise RuntimeError("V272 partially applied")
  print("V272_POSITIVE_BETA_DRAW_REQUEST_ROUTING_ORACLE_PASS");return
 if s.count(A)!=1:
  raise RuntimeError("V272 DRAW REQUEST exact b12 preimage missing")
 if not a.apply:raise RuntimeError("V272 overlay not applied")
 P.write_text(s.replace(A,B,1),encoding="utf8",newline="\n")
 print("V272_POSITIVE_BETA_DRAW_REQUEST_ROUTING_APPLIED")
if __name__=="__main__": main()
