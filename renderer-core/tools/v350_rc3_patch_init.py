#!/usr/bin/env python3
from pathlib import Path
import sys
p=Path(__file__).resolve().parents[2]/"renderer-core/integrated/integrated_addon.cpp"
s=p.read_text()
needle="#include <d3d11.h>"
assert s.count(needle)==1
s=s.replace(needle,needle+'\n#include "dsrrl/runtime/v350_rc3_telemetry.hpp"')
needle="    dsrrl::runtime::qpc_bisect::init_from_game_directory();"
assert s.count(needle)==1
s=s.replace(needle,"    dsrrl::runtime::rc3_telemetry::initialize();\n"+needle)
if "--check" not in sys.argv:p.write_text(s)
print("RC3_INIT_OK")
