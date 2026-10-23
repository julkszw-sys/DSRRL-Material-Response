#!/usr/bin/env python3
"""V315: connect exact 749 sidecar runtime ledger to original draw lifecycle.

Run after V314 complete materialization. Never change the PS/CB/SRV
transaction or treat 75 MTD source candidates as 749 active sidecars.
"""
from pathlib import Path
r=Path(__file__).resolve().parents[1]
def patch(rel,old,new,expected=1):
    p=r/rel;s=p.read_text("utf-8")
    assert s.count(old)==expected,(rel,s.count(old),expected)
    p.write_text(s.replace(old,new),"utf-8")
hdr="include/dsrrl/runtime/material_resource_draw_runtime.hpp"
patch(hdr,
    "    bool spec_rgb = false;",
    "    std::uint64_t logical_spec_hash = 0u; // exact current native t1 source, V315 evidence only\n    bool spec_rgb = false;")
src="src/runtime/material_resource_draw_runtime.cpp"
patch(src,
    "                prepared.spec_rgb = true;",
    "                prepared.spec_rgb = true;\n                prepared.logical_spec_hash = h1; // original stock t1 logical hash and prepared PTDE companion")
v290="include/dsrrl/runtime/v290_spec_draw_census.hpp"
patch(v290,
    '#include "dsrrl/runtime/material_resource_draw_runtime.hpp"',
    '#include "dsrrl/runtime/material_resource_draw_runtime.hpp"\n#if defined(DSRRL_V315_DIRECT_749_RUNTIME_LEDGER)\n#include "dsrrl/runtime/v315_direct_749_sidecar_ledger.hpp"\n#endif')
patch(v290,
'''    if(!probe.logical_hash_allowed)return;
    char buf[256]{};''',
'''#if defined(DSRRL_V315_DIRECT_749_RUNTIME_LEDGER)
    dsrrl::runtime::v315::observe_probe(probe,false,false);
#endif
    if(!probe.logical_hash_allowed)return;
    char buf[256]{};''')
patch(v290,
'''{
    const char *cut =
        !probe.stock_bound''',
'''{
#if defined(DSRRL_V315_DIRECT_749_RUNTIME_LEDGER)
    dsrrl::runtime::v315::observe_probe(
        probe,m.valid && m.owner_tuple_exact,atomic_consumer_authorized);
#endif
    const char *cut =
        !probe.stock_bound''')
addon="integrated/integrated_addon.cpp"
patch(addon,
    '#include "dsrrl/runtime/texture_identity_transport.hpp"',
    '#include "dsrrl/runtime/texture_identity_transport.hpp"\n#if defined(DSRRL_V315_DIRECT_749_RUNTIME_LEDGER)\n#include "dsrrl/runtime/v315_direct_749_sidecar_ledger.hpp"\n#endif')
patch(addon,
    'void on_destroy_device(reshade::api::device *device)\n{\n',
    '''void on_destroy_device(reshade::api::device *device)
{
#if defined(DSRRL_V315_DIRECT_749_RUNTIME_LEDGER)
    dsrrl::runtime::v315::summary();
#endif
''')
before='''#if defined(DSRRL_V291_MATERIAL_STAGE_CENSUS)
    dsrrl::runtime::v291_material_trace::log(
        "batch_prepared",material,decision,receiver_id,''';
after='''#if defined(DSRRL_V315_DIRECT_749_RUNTIME_LEDGER)
    if(prepared.envspec_in_batch)
        dsrrl::runtime::v315::prepared(
            prepared.envspec.material_resources.logical_spec_hash);
#endif
#if defined(DSRRL_V291_MATERIAL_STAGE_CENSUS)
    dsrrl::runtime::v291_material_trace::log(
        "batch_prepared",material,decision,receiver_id,'''
patch(addon,before,after,expected=2)
before='''#if defined(DSRRL_V291_LERP_CUBEMAP_CENSUS)
    if (prepared.envspec_in_batch)
        dsrrl::runtime::v291_material_trace::log_lerp_cubemaps(
            dsrrl::runtime::draw_tx_issued(dispatch.transaction)'''
after='''#if defined(DSRRL_V315_DIRECT_749_RUNTIME_LEDGER)
    // A prepared PTDE sidecar is never counted as issued. The actual
    // transaction result distinguishes draw dispatch and restore failure.
    if(prepared.envspec_in_batch &&
       dsrrl::runtime::draw_tx_issued(dispatch.transaction))
        dsrrl::runtime::v315::issued(
            prepared.envspec.material_resources.logical_spec_hash,
            hemenvlerp_bound,
            dispatch.transaction !=
                dsrrl::runtime::draw_tx_result::issued_restore_failed);
#endif
#if defined(DSRRL_V291_LERP_CUBEMAP_CENSUS)
    if (prepared.envspec_in_batch)
        dsrrl::runtime::v291_material_trace::log_lerp_cubemaps(
            dsrrl::runtime::draw_tx_issued(dispatch.transaction)'''
patch(addon,before,after,expected=2)
print("V315_749_DIRECT_NAME_ACTUAL_DRAW_RESULT_LEDGER_INTEGRATED_PASS")
print("V315_GUARDS=NO_MUTATION_NO_LOW_LOD_ALIAS_NO_RECEIVER_AUTHORIZATION")
