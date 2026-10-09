#!/usr/bin/env python3
import argparse
from pathlib import Path

def fail(msg):
    raise RuntimeError(msg)

def require(text, needle, label):
    if needle not in text:
        fail(f"{label}: missing {needle!r}")

ap=argparse.ArgumentParser()
ap.add_argument("--source-dir",required=True)
ns=ap.parse_args()
root=Path(ns.source_dir).resolve()

integrated=(root/"integrated/integrated_addon.cpp").read_text(encoding="utf-8")
source_h=(root/"include/dsrrl/runtime/pmetal_env_source_runtime.hpp").read_text(encoding="utf-8")
source_cpp=(root/"src/runtime/pmetal_env_source_runtime.cpp").read_text(encoding="utf-8")
producer_cpp=(root/"src/runtime/pmetal_producer_state.cpp").read_text(encoding="utf-8")
env_h=(root/"include/dsrrl/runtime/pmetal_envspec_draw_runtime.hpp").read_text(encoding="utf-8")
env_cpp=(root/"src/runtime/pmetal_envspec_draw_runtime.cpp").read_text(encoding="utf-8")
flver_cpp=(root/"src/runtime/flver_engine_hooks.cpp").read_text(encoding="utf-8")
lerp_cpp=(root/"src/operators/env_spec/pmetal_rgba_lerp_materializer.cpp").read_text(encoding="utf-8")

# Exact P_Metal selector carrier remains the preferred path. Normalize
# whitespace so the audit verifies semantic calls rather than C++ formatting.
selector_begin=source_cpp.index("void pmetal_env_source_selector_event(")
selector_end=source_cpp.index("bool pmetal_env_source_runtime::latest(",selector_begin)
selector_compact="".join(source_cpp[selector_begin:selector_end].split())
for needle in [
    "exact_pmetal_material_selection(",
    "pmetal_selector_policy::select(",
    "pmetal_selector_policy::source(",
    "read_exact_source(",
    "pmetal_producer_state_publish(",
]:
    require(selector_compact,needle,"exact selector carrier")
require(producer_cpp,"thread_local producer_record g_record","selector TLS source state")
require(producer_cpp,"same_material(","selector source material identity")
require(producer_cpp,"same_source_payload(","selector source generation semantics")
require(flver_cpp,"pmetal_env_source_selector_event(","FLVER selector source bridge")

# Restored EnvSpec-only source semantic cut: exact retail single/blend LightBank
# packers from V13, with preimage-guarded install and verified restoration.
for needle in [
    "k_envspec_single_rva = 0x563B80u",
    "k_envspec_blend_rva = 0x563C30u",
    "k_envspec_single_preimage",
    "k_envspec_blend_preimage",
    "prepare_source_hook(",
    "arm_source_hook(",
    "restore_source_hook(",
    "envspec_single_hook_entry(",
    "envspec_blend_hook_entry(",
    "install_envspec_source_hooks(",
    "uninstall_envspec_source_hooks()",
]:
    require(source_cpp,needle,"narrow retail EnvSpec source hooks")

# The hooks may only publish PTDE donor data resolved by the existing exact
# LightBank donor authority. They must call the original retail function too.
single=source_cpp[source_cpp.index("void __fastcall envspec_single_hook_entry("):source_cpp.index("void __fastcall envspec_blend_hook_entry(")]
blend=source_cpp[source_cpp.index("void __fastcall envspec_blend_hook_entry("):source_cpp.index("bool install_envspec_source_hooks(")]
for part,label in ((single,"single"),(blend,"blend")):
    require(part,"read_exact_source(","PTDE donor decode "+label)
    require(part,"_original(","retail original call "+label)
    require(part,"publish_hook_source(","narrow source publish "+label)

# Draw consumption is exact-material-scoped. Selector TLS is preferred; the
# retail source-hook record is a persistent generation-stamped fallback,
# matching the validated V13 producer-state lifetime and never generic U/L state.
consumer=source_cpp[source_cpp.index("bool pmetal_env_source_runtime::latest("):source_cpp.index("pmetal_env_source_runtime_telemetry")]
require(consumer,"!exact_pmetal_material_selection(","exact P_Metal gate")
require(consumer,"pmetal_producer_state_latest(","selector source first")
require(consumer,"latest_hook_source(out)","retail source fallback")
if consumer.index("latest_hook_source(out)") < consumer.index("!exact_pmetal_material_selection("):
    fail("retail hook source can be consumed before exact P_Metal material gate")
# EnvSpec source hooks are independent from visible Upper/Lower. U/L remains
# disabled by runtime policy unless separately enabled.
require(integrated,
        "const bool lightbank_reference_transport_required =\n        upper_lower_enabled;",
        "U/L-only reference hook ownership")
require(integrated,
        "narrow retail LightBank single/blend PTDE source fallback ACTIVE",
        "EnvSpec source carrier status")
for forbidden in [
    "upper_lower_draw_runtime",
    "prepared_upper_lower_draw",
    "prepare_upper_lower_carrier",
    "operator_id::upper_lower",
]:
    if forbidden in env_h or forbidden in env_cpp:
        fail("P_Metal EnvSpec draw runtime depends on visible U/L: "+forbidden)

# EnvSpec draw still enforces exact material/route/receiver, semantic slot,
# source, replacement shader, probe, SpecRGB and b12 readiness.
for needle in [
    # Exact-MTD material+MR-decision policy is now one strict predicate.
    "exact_pmetal_envspec_candidate(",
    # Newly admitted MTDs must never consume the unkeyed LightBank fallback.
    "is_experimental_ptde_metal_envspec_material(material)",
    "source_.latest_exact_material(material, source)",
    # Only baseline P_Metal retains the historically validated fallback.
    "source_.latest(material, source)",
    "effect_source_ready_",
    "effect_replacement_ready_",
    "effect_probe_ready_",
    "effect_spec_rgb_ready_",
    "effect_request_ready_",
]:
    require(env_cpp,needle,"EnvSpec activation gate")

# Lerp payload must preserve stock U/L while EnvSpec owns its own shader/CB/SRV.
require(lerp_cpp,
        "outcome.upper_lower_preserved_stock =\n        !compose_upper_lower;",
        "stock U/L preserved in EnvSpec Lerp")
if "install_provider_splice" in source_cpp or "DSRRL_Material_Response_1.0.addon64" in source_cpp:
    fail("legacy V13 addon splice resurrected")

print("DSRRL_PMETAL_ENVSPEC_REFERENCE_TRANSPORT_PASS")
print("  source=exact selector first, V13-attested retail single/blend LightBank fallback second")
print("  fallback=exact P_Metal scoped + generation-stamped persistent V13 state; visible U/L remains stock/off")
print("  runtime activation and pixel behavior remain OPEN")
