from pathlib import Path
p=Path(__file__).resolve().parents[1]
assert (p/'include/dsrrl/runtime/runtime_hot_telemetry.hpp').exists()
print('V413 diagnostic source exists')
