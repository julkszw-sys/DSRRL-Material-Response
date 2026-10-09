#!/usr/bin/env python3
"""Exact-source R43 RC1 P_Metal map provenance guard (build-time materialization).

No engine executable changes, global gains or area overrides. The unchanged
RC1 C++ file is patched only when its exact git blob SHA1 matches 410e4591.
The guard fails open on missing or cross-area GI/LightBank provenance.
"""
import hashlib
from pathlib import Path

SOURCE = Path(__file__).resolve().parents[1] / "src/runtime/pmetal_envspec_draw_runtime.cpp"
EXPECTED_BLOB = "277ba7191182e5d19fbbd07c0874cfb6a72e8168"
MARKER = "    effect_latch(effect_probe_ready_);"
INCLUDE = '#include "dsrrl/runtime/pixel_srv_shadow.hpp"'
NEW_INCLUDE = '#include "dsrrl/runtime/pmetal_map_provenance.hpp"'
GUARD = """    // RC1 R43 P_Metal receiver-local semantic cut:
    // the authenticated bank AND bound GI probe must belong to the same map.
    // No bank guessing, latest-source reuse, interpolated gain or global patch.
    if (!pmetal_map_provenance::match(
            source.bank_signature_a,source.bank_signature_b,
            prepared.env_resources.probe_a,prepared.env_resources.probe_b,
            probe_b_required)) {
        if (shader != nullptr) shader->Release();
        env_resources_.release(prepared.env_resources);
        telemetry::hot_count(probe_rejects_);
        effect_fail(effect_fail_mask_,k_effect_fail_probe);
        log_prepare_stage_once(1u << 14u,"cross_map_lightbank_probe",
                               material,decision,family);
        return false;
    }
"""

def git_sha(data):
    return hashlib.sha1(b"blob " + str(len(data)).encode() + b"\0" + data).hexdigest()

def patch(data):
    if git_sha(data) != EXPECTED_BLOB:
        raise ValueError(f"RC1 exact P_Metal source identity mismatch: {git_sha(data)}")
    text = data.decode("utf-8")
    if text.count(INCLUDE) != 1 or text.count(MARKER) != 1:
        raise ValueError("R43 expected single header/probe-ready insertion point")
    text = text.replace(INCLUDE, INCLUDE + "\n" + NEW_INCLUDE, 1)
    text = text.replace(MARKER, GUARD + MARKER, 1)
    assert text.count("pmetal_map_provenance::match(") == 1
    return text.encode("utf-8")

def main():
    import argparse
    cli=argparse.ArgumentParser()
    cli.add_argument("--check",action="store_true")
    args=cli.parse_args()
    data=SOURCE.read_bytes()
    result=patch(data)
    # A deliberately repeated patch MUST be rejected.
    try:
        patch(result)
    except ValueError:
        pass
    else:
        raise AssertionError("Already-patched file erroneously accepted")
    if not args.check:
        SOURCE.write_bytes(result)
    print("PASS R43 exact-source guard", git_sha(data), "->",git_sha(result),
          "check_only="+str(args.check))

if __name__=="__main__":
    main()
