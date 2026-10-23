#!/usr/bin/env python3
from pathlib import Path
p=Path('renderer-core/src/runtime/material_resource_draw_runtime.cpp')
s=p.read_text('utf-8')
a='    if (!spec_member &&\n        !diffuse_member &&\n        !normal_member)\n        return;'
b='    // Named stock identities are tracked independently of PTDE eligibility.\n    const bool identity_only = !spec_member && !diffuse_member && !normal_member;\n    if (identity_only && logical_hash == 0u) return;'
assert s.count(a)==1,'ID_GATE_DRIFT'
p.write_text(s.replace(a,b,1),'utf-8')
print('STOCK_IDENTITY_PASS')
