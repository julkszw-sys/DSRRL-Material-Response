#!/usr/bin/env python3
"""Source contract ONLY, performance improvement awaits in-game A/B."""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
overlay=(root/"tools/v468_low_overhead_overlay.py").read_text(encoding="utf-8")
prep=(root/"tools/v468_prepare_build.py").read_text(encoding="utf-8")
name=(root/"tools/v468_name_binary.py").read_text(encoding="utf-8")
for marker in ("if(false && v465_prep_chrins())",
 "CHRINS_OWNER_HOOK_DISABLED_PERF",
 "source_hook_call=DISABLED_V468_NOT_TESTED",
 "WorldChrMan typed probe active only",
 "receiver handle read active only",
 "state().active.load(",
 "state().finished.load(",
 "note_rx34() noexcept",
 "original_game_code=1 no_native_detour=1"):
 assert marker in overlay,marker
for tag in ("V415 MODEL SELECTOR","V416 FGMODEL PARENT","V417 FG REGISTRY PROVENANCE","V415 MODEL SOURCE JOIN"):
 assert tag in overlay,tag
assert "if (number<=8u || number%8192u==0u)" in overlay
assert "if (reg_n<=8u || reg_n%8192u==0u)" in overlay
assert "if (n<=8u || n%8192u==0u)" in overlay
assert "if (fg_number<=8u || fg_number%8192u==0u)" in overlay
assert "import v468_low_overhead_overlay" in prep
assert "DSRRL_V468_RX34_LowOverhead_WorldPlayer.addon64" in name
for forbidden in ("DrawIndexed(","set_lightbank(","shader_patch(","DrawIndexedInstanced("):
 assert forbidden not in overlay,forbidden
print("V468 no native hook + active-window-only probe source contract PASS; gameplay FPS OPEN")
