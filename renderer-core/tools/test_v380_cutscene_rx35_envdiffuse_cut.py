from pathlib import Path
s=(Path(__file__).resolve().parents[1]/'src/runtime/pmetal_envspec_draw_runtime.cpp').read_text()
a=s.index('bool pmetal_envspec_draw_runtime::prepare_independent_envdiffuse(')
b=s.index('#endif\n\nbool pmetal_envspec_draw_runtime::prepare(',a)
f=s[a:b]
assert '[DSRRL V380 RX35 ED CUT]' in f
assert 'decision.receiver_id!=35u || material.route_index!=345u' in f
assert 'source_or_domain_reject' in f
assert 't11_exact_resource_reject' in f
assert 'shader_missing' in f
assert 'request_ready' in f
assert 'const bool source_ok=source_.latest(material,source);' in f
assert f.count('source_.latest(material,source)')==1
assert 'seen.fetch_or(bit,std::memory_order_relaxed)' in f
assert 'source.beta!=0.0f' in f
assert 'env_resources_.prepare_envdiffuse_bound(' in f
assert 'q.srvs[0]={11u,prepared.envdiffuse_resources.ptde_a};' in f
assert 'if(!exact_resource){release(prepared);return false;}' in f
print('V380_CUTSCENE_RX35_SOURCE1_STAGE_WITNESS_SOURCE_PASS')
