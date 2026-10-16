#!/usr/bin/env python3
"""V333 source/identity guard; no runtime pixel claim.

The witness is a second R8 shader. Its only changed semantic is the original
DSR Schlick final MAD on exact RX33/34/35. The material gate must be exact
owner BD_M_9530_M slot9 P_DullLeather, never slot4 P_Leather.
"""
from pathlib import Path
import re

ROOT=Path(__file__).resolve().parents[1]
SRC=(ROOT/"src/operators/env_spec/pmetal_rgba_lerp_materializer.cpp").read_text(encoding="utf-8")
RUNTIME=(ROOT/"src/runtime/pmetal_envspec_draw_runtime.cpp").read_text(encoding="utf-8")
HEADER=(ROOT/"include/dsrrl/runtime/pmetal_envspec_draw_runtime.hpp").read_text(encoding="utf-8")
BUILD=(ROOT/"tools/build_v333_9530_slot9_schlick_one_addon.ps1").read_text(encoding="utf-8")

FLVER="1398eec9e1545fb52688c691d1ff3caa042de3750bccc6b267165a136c3ca845"
MTD="7ea7d6ce9bb7148db62e8c27ee45edefb9547018d0b723dd4753d4c37e29f293"
def cxx_literals(s):
 vals=re.findall(r"0x([a-fA-F0-9]{2})u",s)
 return "".join(vals).lower()

assert "materialize_v333_nonmetal_r8_schlick_isolation_candidate" in SRC
assert "k_neg5_55473 = 0xc0b1c059u" in SRC
assert "k_neg6_98316 = 0xc0df760cu" in SRC
assert "words[site_offset]    = 0x05000036u" in SRC
assert "words[site_offset+3u] = 0x00100246u" in SRC
assert "std::fill(words.begin()+static_cast<std::ptrdiff_t>(site_offset+5u)" in SRC
assert "legacy_plan::dxbc::checksum_container_valid" in SRC
assert SRC.count("materialize_v333_nonmetal_r8_schlick_isolation_candidate(")==1
assert "materialize_nonmetal_lerp_r8_spec_linear_candidate(" in SRC
assert "DSRRL_V333_WANDERER_SLOT9_SCHLICK_WITNESS" in SRC

for key,expected in (("k_wanderer_flver",FLVER),("k_dull_raw_mtd",MTD)):
 match=re.search(r"static constexpr core::sha256_digest "+key+r"\s*=\s*\{([^}]+)\}",RUNTIME)
 assert match,key
 assert cxx_literals(match.group(1))==expected,(key,cxx_literals(match.group(1)))
assert "material.material_slot == 9u" in RUNTIME
assert "material.owner_tuple_exact" in RUNTIME
assert "material.actual_material_exact" in RUNTIME
assert "material.raw_mtd_sha256 == k_dull_raw_mtd" in RUNTIME
assert "material.flver_sha256 == k_wanderer_flver" in RUNTIME
assert "P_DullLeather[DSB].mtd" in RUNTIME
assert "v333_slot9_schlick_selected" in RUNTIME
assert "v289_r8_ps_selected" in RUNTIME
assert "lerp_nonmetal_r8_v333_schlick_replacements_" in HEADER
assert "CreatePixelShader" in RUNTIME
assert "V333_WANDERER_SLOT9_SCHLICK_WITNESS=ON" in BUILD
assert "V289_NONMETAL_LERP_R8_SPEC_LINEAR_AB=ON" in BUILD
assert "FULL_TELEMETRY_DEFAULT_ON=OFF" in BUILD
assert "PTDE" in SRC
assert "source_g=shape.source_g" in SRC

# Source original 3 RX target late Schlick has a unique register shape.
original = {
 33: (12,3,7,0x00100556,11),
 34: (11,3,8,0x00100ff6,10),
 35: (10,3,7,0x00100ff6,9)
}
shape_match=re.search(r"constexpr site_shape shapes\[3\]\s*=\s*\{([\s\S]*?)\};",SRC)
assert shape_match
numbers=[tuple(int(v[:-1],16) if v.startswith("0x") else int(v[:-1])
    for v in xs.split(","))
    for xs in re.findall(r"\{([^{}]+)\}",shape_match.group(1))]
assert len(numbers)==3
assert numbers==list(original.values()),numbers
print("V333 source PASS: RX33/34/35 exact Schlick shape, nonmetal R8-derived candidate, 256bit model+MTD slot9 gate, slot4 untouched.")
print("CONSTRUCTION_ONLY. A material's received selection log and pixel appearance still require owner runtime proof.")
