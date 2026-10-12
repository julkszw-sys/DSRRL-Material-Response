#!/usr/bin/env python3
"""Source oracle for V244, including both callback forms and safety gates."""
import importlib.util
from pathlib import Path
import re

ROOT = Path("renderer-core/integrated/integrated_addon.cpp")
V243 = Path("renderer-core/tools/apply_v243_rx34_draw_proof.py")
V244 = Path("renderer-core/tools/apply_v244_true_lerp_draw_proof.py")

def load(path, name):
    spec = importlib.util.spec_from_file_location(name, path)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod

old = load(V243, "v243_apply")
new = load(V244, "v244_apply")
source = ROOT.read_text(encoding="utf-8")
assert "[DSRRL V243 RX DRAW PROOF]" not in source
assert "[DSRRL V244 TRUE LERP DRAW PROOF]" not in source
overlay = old.patch(source, True)
assert overlay.count("[DSRRL V243 RX DRAW PROOF]") == 1

changed = new.patch(overlay)
assert changed == new.patch(changed), "V244 is not idempotent"
assert "[DSRRL V243 RX DRAW PROOF]" not in changed
assert changed.count("[DSRRL V244 TRUE LERP DRAW PROOF]") == 1
assert changed.count("v244_true_lerp_draw_proof(") == 7
assert len(re.findall(
    r"v244_true_lerp_draw_proof\(\s*receiver_id,\s*"
    r"decision\.route_index,\s*decision\.active,\s*"
    r"hemenvlerp_bound,\s*material\.material_slot,\s*"
    r"material\.semantic_name_hash", changed)) == 6

# Bounded and independent per exact route, with no profiler/QPC/native calls.
start = changed.index("void v244_true_lerp_draw_proof(")
end = changed.index("\n}\n", start)
fn = changed[start:end]
for token in (
    "!exact_lerp_bound",
    "std::uint64_t{1} << bit_index",
    "(rx - 33u) * 20u + route_lane * 4u + stage",
    "route == 345u",
    "route == 160u",
    "route == 1073741869u",
    "route == 232u",
    "static thread_local std::uint64_t local_seen",
    "std::atomic<std::uint64_t> global_seen",
    "QPC=OFF pixel=OPEN",
):
    assert token in fn, token
for forbidden in (
    "QueryPerformanceCounter",
    "PSGetShaderResources",
    "GetResource(",
    "draw_indexed(",
    "dispatch_island",
):
    assert forbidden not in fn, forbidden

# Functional original shader, producer, GPU, CB, SRV, and draw sites untouched.
def strip_v243(text):
    i = text.index("void v243_rx34_draw_proof(")
    j = text.index("\n}\n", i) + 3
    return text[:i] + text[j:]
def strip_v244(text):
    i = text.index("void v244_true_lerp_draw_proof(")
    j = text.index("\n}\n", i) + 3
    return text[:i] + text[j:]
# Verify only the three explicit stage callsets + observer differ.
v243_tail = strip_v243(overlay)
v244_tail = strip_v244(changed)
v244_tail = re.sub(
    r"v244_true_lerp_draw_proof\(\s*receiver_id,\s*"
    r"decision\.route_index,\s*decision\.active,\s*"
    r"hemenvlerp_bound,\s*material\.material_slot,\s*"
    r"material\.semantic_name_hash,\s*",
    "v243_rx34_draw_proof(\n            "
    "receiver_id, decision.route_index, decision.active,\n            ",
    v244_tail
)
assert re.sub(r"\s+", "", v244_tail) == re.sub(r"\s+", "", v243_tail), (
    "Unexpected non-observer rendering difference")
print("V244_TRUE_LERP_FAMILY_STAGED_CALLBACK_ORACLE_PASS")
print("V244_ONLY_OBSERVER_DIFF_NO_QPC_NO_RENDERER_MUTATION_PASS")
