#!/usr/bin/env python3
from pathlib import Path
p=Path('renderer-core/src/runtime/pmetal_env_source_runtime.cpp')
s=p.read_text()
old='''        if (row_id >= bank.count)
            return false;
'''
new='''        if (row_id >= bank.count) {
            const auto *extra = dsr_only_lightbank_rows_v1::find(signature,row_id);
            if (extra == nullptr) return false;
            const float scale = static_cast<float>(extra->m) * 0.01f;
            out = {extra->r / 255.0f * scale, extra->g / 255.0f * scale,
                   extra->b / 255.0f * scale, 0.0f};
            return std::isfinite(out.x) && std::isfinite(out.y) &&
                   std::isfinite(out.z);
        }
'''
assert s.count(old)==1
p.write_text(s.replace(old,new,1))
print('PASS exact DSR-only EnvDiffuse row64')
