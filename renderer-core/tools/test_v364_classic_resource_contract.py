from pathlib import Path
r=Path(__file__).resolve().parents[1]
s=(r/'src/runtime/envspec_resource_runtime.cpp').read_text()
d=(r/'tools/materialize_v230_envdiffuse_cinematic_trace.py').read_text()
x=s.split('bool envspec_resource_runtime::prepare_classic_envdiffuse_bound(',1)[1].split('\n#endif',1)[0]
for key in ['native_legacy_envdiffuse_for_view(stock_t11,a)','get_ptde_classic_candidate(name_a,candidate_a)','native_a->AddRef()','native_b->AddRef()','sampler->AddRef()','out.ready=true']:
 assert key in x,key
for key in ['PSSetShaderResources','PSSetShader(','prepare_native_dsr','g_diffuse_by_view.find','g_envdiffuse_pack_ready']:
 assert key not in x,key
assert 'env_resources_.release(acquired_classic);' in d
assert 'envspec_t12=REJECTED bind=NO pixels=OPEN' in d
print('V364_CLASSIC_RESOURCE_ACQUISITION_PASS pixel=OPEN')
