#!/usr/bin/env python3
"""V243 exact source and no producer/shader mutation negative oracle."""
from pathlib import Path
import importlib.util
path=Path(__file__).with_name("apply_v243_rx34_draw_proof.py")
spec=importlib.util.spec_from_file_location("v243",path)
m=importlib.util.module_from_spec(spec)
spec.loader.exec_module(m)
root=Path(__file__).resolve().parents[2]
source=(root/m.PATH).read_text(encoding="utf-8")
assert m.patch(source)==source
new=m.patch(source,True)
assert new!=source
assert m.patch(new,True)==new
assert new==source.replace(m.GLOBAL,m.GLOBAL_ADD,1).replace(m.FAIL,m.FAIL_ADD,2).replace(m.READY,m.READY_ADD,2).replace(m.ISSUE,m.ISSUE_ADD,2)
assert "stage=REQUEST_READY_NOT_PIXEL" not in m.GLOBAL_ADD
assert "stage=%s" in m.GLOBAL_ADD
assert "PREPARE_FAIL_STOCK" in m.GLOBAL_ADD
assert "REQUEST_BATCH_READY" in m.GLOBAL_ADD
assert "DRAW_ISSUED" in m.GLOBAL_ADD
assert "DISPATCH_FAIL_STOCK" in m.GLOBAL_ADD
assert "rx != 33u && rx != 34u" in m.GLOBAL_ADD
assert "static thread_local" in m.GLOBAL_ADD
assert "if ((local_seen & bit) != 0u)" in m.GLOBAL_ADD
assert "draw_tx_issued(dispatch.transaction)" in m.ISSUE_ADD
assert "prepared.envspec_in_batch" in m.READY_ADD
for bad in (source.replace(m.GLOBAL,"missing"),
            source.replace(m.ISSUE,"moved"),
            source.replace("dispatch_island_draw_indexed_batch(","bad(")):
    try:m.patch(bad,True)
    except ValueError:pass
    else:raise AssertionError("V243 did not reject changed source")
print("V243_RX34_PREPARED_VS_ISSUED_CONSTRUCTION_CONTRACT_PASS")
