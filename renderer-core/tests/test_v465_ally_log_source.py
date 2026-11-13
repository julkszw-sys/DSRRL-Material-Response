#!/usr/bin/env python3
"""V465 native-addon source contract (synthetic only, NOT runtime actor proof)."""
from pathlib import Path
from importlib.util import spec_from_file_location,module_from_spec
R=Path(__file__).resolve().parents[1]
o=(R/"tools/v465_actor_owner_overlay.py").read_text(encoding="utf-8")
a=(R/"tools/v465_ally_log_overlay.py").read_text(encoding="utf-8")
p=(R/"tools/v465_prepare_build.py").read_text(encoding="utf-8")
n=(R/"tools/v465_name_binary.py").read_text(encoding="utf-8")
assert "0x321CE0u" in o and "k_v465_chrins_preimage" in o
assert "g_base+0x13251F0u" in o and "g_base+0x1322E68u" in o
assert "g_base+0x12EAF30u" in o
assert "model+0x10u" in o and "entity459+0x10u" in o
assert "v465_arm_chrins()" in o and "restore(g_v465_chrins)" in o
assert "ALLy_AUTO_CAPTURE_START".lower() in a.lower()
assert "RX34>=16" in a and "25000u" in a
assert "ACTOR_HANDLE status=%s" in a and "SUMMARY handles=%u" in a
assert "no_CSV=1" in a and "no_keyboard=1" in a
assert "GetAsyncKeyState" not in a
assert "v465::observe_draw" in a and "v465::observe_owner" in a
assert "V451_MODEL_RESOURCE_AT_ISSUE" in o
assert "import v465_ally_log_overlay" in p
assert "DSRRL_V465_Ally_Auto_Actor_RX34_LogOnly.addon64" in n
for x in [o,a]:
    for prohibited in ["latest_hook_source(", "set_lightbank(", "shader_patch(", "DrawIndexed(", "DrawIndexedInstanced("]:
        assert prohibited not in x,prohibited
print("V465 synthetic source contract PASS, game-runtime actor proof still OPEN")
