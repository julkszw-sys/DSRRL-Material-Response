#!/usr/bin/env python3
"""V372 exact stable EnvDiffuse bound-t11 bridge in the combined shader.
Run AFTER V359 and BEFORE native compilation. No LightBank/gain alteration.
"""
from pathlib import Path
p=Path(__file__).resolve().parents[1]/"src/runtime/pmetal_envspec_draw_runtime.cpp"
s=p.read_text("utf-8")
a='''    if (!env_resources_.prepare_envdiffuse(
            prepared.env_resources.probe_a,
            prepared.env_resources.probe_b,
            ptde_envdiffuse_b_required,
            prepared.envdiffuse_resources)) {'''
b='''    bool envdiffuse_bound_ready=false;
#if defined(DSRRL_V370_ISOLATED_EXACT_ENVDIFFUSE)
    if (family==pmetal_envspec_receiver_family::stable_hemenv) {
        ID3D11ShaderResourceView *t11[1]{};
        if(!pixel_srv_shadow_snapshot(cmd_list,11u,1u,t11)) {
            if(t11[0]!=nullptr) {t11[0]->Release();t11[0]=nullptr;}
            context->PSGetShaderResources(11u,1u,t11);
        }
        // Independent native t11 SHA->PTDE EnvDiffuse atlas, not t12.
        envdiffuse_bound_ready=env_resources_.prepare_envdiffuse_bound(
            t11[0],nullptr,false,prepared.envdiffuse_resources);
        if(t11[0]!=nullptr)t11[0]->Release();
    } else
#endif
    {
        // Lerp endpoint B retains its separate, unverified routing policy.
        envdiffuse_bound_ready=env_resources_.prepare_envdiffuse(
            prepared.env_resources.probe_a,
            prepared.env_resources.probe_b,
            ptde_envdiffuse_b_required,
            prepared.envdiffuse_resources);
    }
    if (!envdiffuse_bound_ready) {'''
if s.count(a)!=1:raise SystemExit("V372 expected exactly one consumer anchor, got "+str(s.count(a)))
s=s.replace(a,b,1)
p.write_text(s,"utf-8")
print("V372_STABLE_ENVDIFFUSE_BOUND_T11_COMPOSITE_PATCH_PASS")
