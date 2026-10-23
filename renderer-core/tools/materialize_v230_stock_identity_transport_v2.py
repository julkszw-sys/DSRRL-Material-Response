#!/usr/bin/env python3
from pathlib import Path
p=Path('renderer-core/src/runtime/material_resource_draw_runtime.cpp')
s=p.read_text('utf-8')
a='    if (!spec_member &&\n        !diffuse_member &&\n        !normal_member)\n        return;'
b='    // Named stock identities are tracked independently of PTDE eligibility.\n    const bool identity_only = !spec_member && !diffuse_member && !normal_member;\n    if (identity_only && logical_hash == 0u) return;'
assert s.count(a)==1,'ID_GATE_DRIFT'
s=s.replace(a,b,1)
preamble='    // std::wstring move is noexcept: do not introduce allocations or'
assert s.count(preamble)==1,'STOCK_IDENTITY_MARKER_DRIFT'
observe='''#if defined(DSRRL_V230_UNGATED_STOCK_IDENTITY)
    if (identity_only) {
        static std::atomic<std::uint32_t> count{0u};
        const auto n=count.fetch_add(1u,std::memory_order_relaxed)+1u;
        if(n<=48u) {
            char msg[200]{};
            std::snprintf(msg,sizeof(msg),
                "[DSRRL STOCK IDENTITY ONLY] n=%u hash=%016llx companions=%u",
                n,static_cast<unsigned long long>(logical_hash),
                (set.specular?1u:0u)|(set.diffuse?2u:0u)|(set.normal?4u:0u));
            reshade::log::message(reshade::log::level::warning,msg);
        }
    }
#endif
'''
s=s.replace(preamble,observe+preamble,1)
p.write_text(s,'utf-8')
print('STOCK_IDENTITY_PASS')
