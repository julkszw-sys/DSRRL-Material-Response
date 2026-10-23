#!/usr/bin/env python3
"""Static soak invariants; run after V351 and V352 materializers."""
from pathlib import Path
r=Path(__file__).resolve().parents[1]
s=(r/"integrated/integrated_addon.cpp").read_text(encoding="utf-8")
h=(r/"include/dsrrl/runtime/v352_long_soak_present_monitor.hpp").read_text(encoding="utf-8")
b=(r/"tools/build_v352_long_soak_telemetry.ps1").read_text(encoding="utf-8")
assert 'g_v352_soak.present()' in s
assert 'g_v352_soak.reset()' in s
assert '(present % 300u)' in s, "historical branch must remain separately gated"
assert 'present == 1u || telemetry_window' in s
assert s.count('log_effect_matrix("LIVE")')==1
assert 'QueryPerformanceCounter(&now)' in h
assert 'QueryPerformanceFrequency(&f)' in h
assert 'frequency_ * 30ull' in h
assert 'std::array<std::uint64_t,201> intervals_' in h
assert 'p95_interval_ms_le=' in h and 'p99_interval_ms_le=' in h
assert 'measurement=PRESENT_INTERVAL_NOT_GPU' in h
assert 'native_state_verify=OFF' in h
assert '-DDSRRL_FULL_TELEMETRY_DEFAULT_ON=ON' in b
assert 'DSRRL_V352_LONG_SOAK_PRESENT' in b
assert 'DSRRL_V351_TYPED_DSR_ONLY_OPERATOR' in b
assert 'DSRRL_V351_EXACT_MATERIAL_ROUTE_RECOVERY' in b
assert '/DDSRRL_QPC_SCOPE_SHELL_NO_CLOCK' not in h
assert 'AddRef()' not in h and 'PSGetShader' not in h
assert 'SetShader' not in h and 'WriteFile' not in h
print('V352_LONG_SOAK_SOURCE_CONTRACT_PASS full counters default ON, QPC per Present, 30s bounded, untouched GPU resources')
