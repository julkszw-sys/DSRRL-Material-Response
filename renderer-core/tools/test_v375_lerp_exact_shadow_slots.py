#!/usr/bin/env python3
"""V375 Lerp resource transport: verify noncontiguous exact EnvSpec slots.

Construction contract only. Does not claim live source activation or PTDE pixel match.
"""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
s=(root/"src/runtime/pmetal_envspec_draw_runtime.cpp").read_text("utf-8")
start=s.index("    // For beta == 0, lerp(A, B, beta) == A.")
end=s.index("    if (!env_resource_ready) {", start)
cut=s[start:end]
assert "const bool probe_b_required = source.beta != 0.0f;" in cut
assert "pixel_srv_shadow_snapshot(\n            cmd_list, 12u, 1u, &shadow_env[0])" in cut
assert "pixel_srv_shadow_snapshot(\n             cmd_list, 14u, 1u, &shadow_env[2])" in cut
assert "shadow_env_a_ready && shadow_env_b_ready" in cut
assert "!probe_b_required ||" in cut
assert "probe_b_required ? 3u : 1u" not in cut
assert "env_resources_.prepare_bound(" in cut
assert "env_resources_.prepare(" in cut
assert "env_resources_.prepare_native_dsr(" in cut
assert "a_ready=%u b_ready=%u" in s
assert "latest_hook_source(" not in cut
print("V375_ENVSPEC_NONCONTIGUOUS_T12_T14_SHADOW_SCOPE_PASS runtime=OPEN pixel=OPEN")
