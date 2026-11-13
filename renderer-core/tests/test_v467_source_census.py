#!/usr/bin/env python3
"""Static contract only. Does not prove a real native PlayerIns RX34 join."""
from pathlib import Path
r=Path(__file__).resolve().parents[1]
p=(r/"tools/v467_owner_probe_overlay.py").read_text(encoding="utf-8")
prep=(r/"tools/v467_prepare_build.py").read_text(encoding="utf-8")
name=(r/"tools/v467_name_binary.py").read_text(encoding="utf-8")
assert "import v467_owner_probe_overlay" in prep
assert "DSRRL_V467_RX34_WorldPlayer_Source_Census.addon64" in name
for marker in (
    "note_chrins_entry","note_chrins_type",
    "OWNER_SOURCE_DIAG","world_queries","world_full_match",
    "TYPED_PLAYER_CANDIDATE", "UNKNOWN_CONFLICT",
    "base459+0x1C77E50u", "world467+0x68u",
    "base459+0x1328210u","base459+0x13251F0u",
    "base459+0x12EAF30u","model467+0x10u",
    "entity459,handle465","static_cast<unsigned>(prepared.source_kind)==1u",
    "world_vt467==base459+0x1328210u",
    "actor_vt467==base459+0x13251F0u",
    "model_vt467==base459+0x12EAF30u",
    "if(player_h467==handle465)flags467|=16u;",
    "observe_world_player(", "v.world_player_matches",
    "rx447.row_a",
):assert marker in p,marker
assert "dsrrl::runtime::v465::observe_owner(" in p
# Explicitly never transform a typed PlayerIns candidate into a V465 owner-hook CPU_JOIN:
assert "DOES NOT promote CPU_JOIN" in p
assert "world_full_mismatch" in p and "world_actor_conflict" in p
for forbidden in ("DrawIndexed(","DrawIndexedInstanced(","set_lightbank(","shader_patch(","latest_hook_source("):
    assert forbidden not in p,forbidden
print("V467 static source census contract PASS; runtime actor and pixels remain OPEN")
