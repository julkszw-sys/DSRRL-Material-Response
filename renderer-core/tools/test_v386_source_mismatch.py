from pathlib import Path
r=Path('renderer-core')
s=(r/'src/runtime/pmetal_env_source_runtime.cpp').read_text()
f=(r/'src/runtime/flver_engine_hooks.cpp').read_text()
h=(r/'include/dsrrl/runtime/v386_source_match.hpp').read_text()
assert 'v386_mismatch(material,out,v386_expected,v386_age)' in s
assert 'v386_publish(identity,a,b,beta)' in f
assert 'action=STOCK_DSR exact_join=1' in s
assert 'a.flver_sha256==b.flver_sha256' in h
assert 'a.raw_mtd_sha256==b.raw_mtd_sha256' in h
assert 'a.semantic_name_hash==b.semantic_name_hash' in h
assert 'c.thread!=GetCurrentThreadId()' in h
assert 'now-c.tick>32u' in h
assert 'src.row_id_a!=src.row_id_b' in h
start=s.index('bool latest_hook_source(')
stop=s.index('void clear_hook_source()',start)
assert 'v386' not in s[start:stop]
print('V386 source mismatch contract PASS')
