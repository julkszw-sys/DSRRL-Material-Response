from pathlib import Path
p=Path("renderer-core/src/runtime/pmetal_env_source_runtime.cpp")
h=Path("renderer-core/include/dsrrl/runtime/pmetal_donor_provenance_policy.hpp")
s=p.read_text().replace("\r\n","\n")
a='#include "dsrrl/runtime/pmetal_env_source_runtime.hpp"'
b="    hook_decode_ok = 9u"
c="    const auto *row = pmetal_env_source_authority::find_row(*bank,row_id);"
assert not h.exists() and all(s.count(t)==1 for t in (a,b,c))
s=s.replace(a,a+'\n#include "dsrrl/runtime/pmetal_donor_provenance_policy.hpp"')
s=s.replace(b,b+",\n    hook_decode_dsr_only = 10u")
guard="""    // V318 DSR_ONLY_ROW_GUARD, original V299 source scope.
    if (pmetal_donor_provenance::classify(row_id,bank->live_count,bank->count)
            == pmetal_donor_provenance::row_origin::dsr_only) {
        record_hook_decode(hook_decode_dsr_only,version_u32,count_u32,index,row_id,signature);
        return false;
    }
"""
s=s.replace(c,guard+c)
p.write_text(s,encoding="utf-8")
h.write_text(Path("renderer-core/tools/v318/pmetal_donor_provenance.hpp.txt").read_text())
print("V318_V299_GUARD_STAGED_ONE_OPERATOR")
