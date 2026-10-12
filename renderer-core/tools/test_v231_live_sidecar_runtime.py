#!/usr/bin/env python3
"""V231 source contract and drift/fail-open regression oracle."""
import importlib.util
from pathlib import Path
path=Path(__file__).with_name("apply_v231_live_sidecar_runtime.py")
spec=importlib.util.spec_from_file_location("v231",path)
a=importlib.util.module_from_spec(spec)
spec.loader.exec_module(a)
assert len(a.CHANGES)>=9
pseudo = ('const std::filesystem::path &process_dir() noexcept\n'
          'void on_destroy_resource_view(\n')
# Reproduce required complete source identities without a 90k fixture.
for old,new in a.CHANGES:
    assert old and new and old!=new
    pseudo+=old+'\n'
pseudo+='    ++g_named_views;\n}\n\nvoid on_destroy_resource_view(\n'
after=a.apply(pseudo)
assert after.count(a.MARKER)==1
assert a.apply(after)==after
for old,new in a.CHANGES:
    assert after.count(new)==1
assert '#include "dsrrl/runtime/live_sidecar_reuse_policy_v230.hpp"' in after
assert "may_reuse_live_sidecar_v230(" in after
assert "g_live_by_logical_hash.clear()" in after
assert "erase_live_companion_index(" in after
assert "candidate.exact_logical_name" not in after  # in pure policy
assert "if (reused)" in after
assert "g_live_companion_reuses.fetch_add(" in after
assert "safe_load_sidecar(" in after
assert "g_ambiguous_view_device.emplace(" in after
for text in [
    pseudo.replace('const std::filesystem::path &process_dir() noexcept','void unproven()'),
    pseudo.replace(a.CHANGES[1][0],"missing struct"),
    pseudo.replace(a.CHANGES[-1][0],"missing counters"),
    pseudo+'std::filesystem::is_regular_file(path)'
]:
    try:
        a.apply(text)
    except ValueError:
        pass
    else:
        raise AssertionError("V231 drift/failure must block compilation")
print("V231_EXACT_LIVE_REUSE_SOURCE_ORACLE_PASS")
