#!/usr/bin/env python3
import importlib.util
from pathlib import Path
p=Path(__file__).with_name('apply_v220_nonmetal_source_identity_join.py')
spec=importlib.util.spec_from_file_location('v220',p)
m=importlib.util.module_from_spec(spec)
spec.loader.exec_module(m)
assert len(m.CHANGES)==4
assert 'actual_material_exact = false' in m.HELPER
print('V220 generator contract PASS')
