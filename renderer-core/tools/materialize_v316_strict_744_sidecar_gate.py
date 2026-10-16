#!/usr/bin/env python3
"""V316: materialize narrow strict-no-low sidecar policy after V315.

Only one equipment SpecRGB name gate is changed (five exact DSR-direct *_l_s
names), and one independent 744-target summary is added. The original V315
749 source ledger, shader gates, FLVER/slot/MTD validation, render transaction,
R43 fallback, PointLight and all PTDE operator paths remain untouched.
"""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
def patch(path, old, new, expected=1):
    target=root/path
    src=target.read_text(encoding="utf-8")
    if src.count(old)!=expected:
        raise RuntimeError(f"V316 source drift: {path}: expected {expected}, got {src.count(old)}")
    target.write_text(src.replace(old,new),encoding="utf-8",newline="\n")

resource="src/runtime/material_resource_draw_runtime.cpp"
patch(resource,
      '#include "dsrrl/runtime/generated_spec_routes_v12.hpp"',
      '#include "dsrrl/runtime/generated_spec_routes_v12.hpp"\n'
      '#if defined(DSRRL_V316_STRICT_NO_LOW_744)\n'
      '#include "dsrrl/runtime/v316_strict_744_sidecar_gate.hpp"\n'
      '#endif')
patch(resource,
      'inline bool spec_equipment_allowed(std::uint64_t hash) noexcept\n{',
      'inline bool spec_equipment_allowed(std::uint64_t hash) noexcept\n{\n'
      '#if defined(DSRRL_V316_STRICT_NO_LOW_744)\n'
      '    // Owner strict-no-low policy takes precedence over legacy V12 allowlist.\n'
      '    // This fails open to STOCK DSR, not to another equipment sidecar.\n'
      '    if(!v316::allow_sidecar_equipment_name(hash))return false;\n'
      '#endif')
addon="integrated/integrated_addon.cpp"
patch(addon,
      '#include "dsrrl/runtime/v315_direct_749_sidecar_ledger.hpp"',
      '#include "dsrrl/runtime/v315_direct_749_sidecar_ledger.hpp"\n'
      '#if defined(DSRRL_V316_STRICT_NO_LOW_744)\n'
      '#include "dsrrl/runtime/v316_strict_744_runtime_summary.hpp"\n'
      '#endif')
patch(addon,
      '    dsrrl::runtime::v315::summary();',
      '    dsrrl::runtime::v315::summary();\n'
      '#if defined(DSRRL_V316_STRICT_NO_LOW_744)\n'
      '    dsrrl::runtime::v316::strict_summary();\n'
      '#endif')
print('V316_SOURCE_MATERIALIZED_STRICT_744_PASS')
print('SCOPE=FIVE_LITERAL_DSR_DIRECT_LOW_NAME_POLICY_AND_READONLY_744_SUMMARY')
