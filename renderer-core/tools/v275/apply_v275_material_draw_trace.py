#!/usr/bin/env python3
"""Transiently attach V275 exact-owner draw probes to V274B/C source pipeline.

Apply AFTER V274B temporarily installs v274b_cb_shadow.hpp. There are no
renderer changes in committed Git; build wrapper restores all overlays.
"""
from pathlib import Path
import argparse
import shutil

ROOT=Path("renderer-core")
SOURCE=ROOT/"integrated/integrated_addon.cpp"
TARGET=ROOT/"include/dsrrl/runtime/v275_material_draw_trace.hpp"
HEADER=ROOT/"tools/v275/v275_material_draw_trace.hpp"
ANCHOR='#include "dsrrl/runtime/v274b_cb_shadow.hpp"\n'
REPL=ANCHOR+'#include <algorithm>\n#include "dsrrl/runtime/v275_material_draw_trace.hpp"\n'
METHODS=(("bool on_draw(\n","bool on_draw_indexed(\n","vertex_count","false"),
         ("bool on_draw_indexed(\n","void on_present(\n","index_count","true"))

def patch_method(block:str,count:str,indexed:str)->str:
    assert "v275::observe" not in block
    id_anchor="""    if (!draw_identity_ready) {
        mark_effect_probe_mask(
            route_candidates,
            effect_probe_stage::fail_open);
        return false;
    }
"""
    assert block.count(id_anchor)==1
    id_repl=id_anchor+f'''
    dsrrl::runtime::v275::observe(
        cmd_list,material,decision,receiver_id,hemenvlerp_bound,
        "IDENTITY",1u,nullptr,{count},{indexed});
'''
    block=block.replace(id_anchor,id_repl,1)

    # The nested V253/V272 overlay may add source operations AFTER a failed
    # prepare; probe at the *exact entry* of its existing failure branch,
    # rather than assuming a fixed trailing block layout from Git HEAD.
    fail_start="""            prepared)) {
"""
    assert block.count(fail_start)==1, "V275 prepare branch preimage drift"
    fail_repl=fail_start+f'''        dsrrl::runtime::v275::observe(
            cmd_list,material,decision,receiver_id,hemenvlerp_bound,
            "PREPARE_FAIL",2u,nullptr,{count},{indexed});
'''
    block=block.replace(fail_start,fail_repl,1)

    batch_needle="""    if (k_pmetal_native_draw_runtime_enabled &&
"""
    assert block.count(batch_needle)==1
    block=block.replace(batch_needle,f'''    dsrrl::runtime::v275::observe(
        cmd_list,material,decision,receiver_id,hemenvlerp_bound,
        "BATCH_PREPARED",3u,&prepared.batch.mutation,{count},{indexed});

'''+batch_needle,1)

    dispatch_needle="""    account_effect_dispatch(
        effect_prepared,
        dispatch.transaction,
        receiver_id,
        decision.route_index);
"""
    assert block.count(dispatch_needle)==1
    dispatch_add=f'''
    dsrrl::runtime::v275::observe(
        cmd_list,material,decision,receiver_id,hemenvlerp_bound,
        dsrrl::runtime::draw_tx_issued(dispatch.transaction)
            ? (dispatch.transaction==dsrrl::runtime::draw_tx_result::
                    issued_restore_failed ? "RESTORE_FAILED":"DRAW_ISSUED")
            : "DISPATCH_FAIL_OPEN",
        dispatch.transaction==dsrrl::runtime::draw_tx_result::
            issued_restore_failed ? 6u:
            dsrrl::runtime::draw_tx_issued(dispatch.transaction) ? 4u:5u,
        nullptr,{count},{indexed});
'''
    block=block.replace(dispatch_needle,dispatch_needle+dispatch_add,1)
    return block

def transform(source:str)->str:
    assert source.count(ANCHOR)==1, "V274B include must precede V275"
    assert source.count(REPL)==0
    source=source.replace(ANCHOR,REPL,1)
    for begin,end,count,indexed in METHODS:
        at=source.index(begin);endat=source.index(end,at+len(begin))
        block=source[at:endat]
        changed=patch_method(block,count,indexed)
        source=source[:at]+changed+source[endat:]
    assert source.count("dsrrl::runtime::v275::observe(")==8
    return source

def main():
    p=argparse.ArgumentParser()
    p.add_argument("--apply",action="store_true")
    args=p.parse_args()
    src=SOURCE.read_text(encoding="utf8")
    if args.apply:
        assert not TARGET.exists(), "Do not overwrite transient owner header"
        assert HEADER.is_file()
        changed=transform(src)
        SOURCE.write_text(changed,encoding="utf8",newline="\n")
        shutil.copyfile(HEADER,TARGET)
        print("V275_TRANSIENT_DRAW_TRACE_APPLIED")
    else:
        assert REPL in src and src.count("dsrrl::runtime::v275::observe(")==8
        assert TARGET.read_bytes()==HEADER.read_bytes()
        print("V275_EXACT_DRAW_OBSERVER_ORACLE_PASS")
    print("V275_NO_MUTATION_TO_PS_CB_SRV_RTV_EXE_STABLE")
if __name__=="__main__":
    main()
