#!/usr/bin/env python3
"""Read-only t11 original retail-name witness contract on rejected EnvSpec draws."""
from pathlib import Path
r=Path(__file__).resolve().parents[1]
src=(r/"tools/materialize_v230_envdiffuse_cinematic_trace.py").read_text("utf8")
v357=(r/"tools/materialize_v357_exact_resource_source_census.py").read_text("utf8")
assert src.count("[DSRRL V359 T11 RETAIL ORIGIN]")==1
assert src.count("v214_resource_provenance::instance().snapshot(")==1
assert 'v214_resource_provenance::status::exact' in src
assert 'v214_resource_provenance::status::ambiguous' in src
assert 'name_status=%s name=%.96s' in src
assert 'mapping=UNAUTHORIZED pixels=OPEN' in src
assert "if(native_device!=nullptr) native_device->Release();" in src
assert "if(res!=nullptr) res->Release();" in src
assert '#include "dsrrl/runtime/v214_resource_name_provenance.hpp"' in v357
assert "if (!env_resource_ready) {" in src
assert 't12=REJECTED' in src and 'action=STOCK_DSR pixels=OPEN' in src
start=src.index('// Exact V214 retail source name, scoped')
end=src.index('if(res!=nullptr) res->Release();',start)
region=src[start:end]
for forbidden in ("PSSetShaderResources(","PSSetSamplers(","DrawIndexed(","CopyResource(","UpdateSubresource(","Map(","Unmap(","probe_a =","probe_b ="):
    assert forbidden not in region,forbidden
print("V359_T11_EXACT_RETAIL_ORIGIN_SOURCE_PASS; receiver/bridge/pixels OPEN")
