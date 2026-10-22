from pathlib import Path
p=Path(__file__).resolve().parents[1]
shader=(p/'src/operators/env_spec/pmetal_rgba_materializer.cpp').read_text()
runtime=(p/'src/runtime/pmetal_envspec_draw_runtime.cpp').read_text()
addon=(p/'integrated/integrated_addon.cpp').read_text()
cmake=(p/'integrated/CMakeLists.txt').read_text()
assert 'materialize_ptde_envdiffuse_stable_only(' in shader
assert 'apply_exact_ptde_envdiffuse_consumer(decl,true)' in shader
assert 'ptde_sample[7]=0x00107936u' in shader
assert 'add_b12_rdef(decl)' in shader
assert 'words[temps+1u]=14u' in shader
assert 'source_.latest(material,source)' in runtime
assert 'payload[12]=source.envdiffuse_a[0]' in runtime
assert 'payload[13]=source.envdiffuse_a[1]' in runtime
assert 'payload[14]=source.envdiffuse_a[2]' in runtime
start=runtime.index('bool pmetal_envspec_draw_runtime::prepare_independent_envdiffuse(')
end=runtime.index('\n#endif',start)
body=runtime[start:end]
assert 'prepare_envdiffuse_bound(' in body
assert 'prepare_envspec(' not in body and 'prepare_native_dsr(' not in body
assert 'q.constant_buffers[0]={12u,prepared.b12,owner};' in body
assert 'q.srvs[0]={11u,prepared.envdiffuse_resources.ptde_a};' in body
assert 'q.samplers[0]={11u,prepared.envdiffuse_resources.sampler};' in body
assert 'operator_id::env_diffuse' in addon
assert 'return false; // fail-open native DSR' in addon
assert 'DSRRL_V365_CLASSIC_RX34_STANDALONE_DXBC' not in cmake
print('V370 exact old PTDE consumer isolation source PASS; GPU & pixels OPEN')
