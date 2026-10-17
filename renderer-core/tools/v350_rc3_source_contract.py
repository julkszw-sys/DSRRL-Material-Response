#!/usr/bin/env python3
from pathlib import Path
root=Path(__file__).resolve().parents[2]
source=(root/'renderer-core/integrated/integrated_addon.cpp').read_text()
assert 'k_pointlight_drawtime_runtime_enabled = false;' in source
print('RC3_SOURCE_BASE_PASS')
