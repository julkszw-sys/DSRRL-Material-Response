#!/usr/bin/env python3
"""Self-contained V459 synthetic data contract (NEVER real game evidence)."""
import csv,importlib.util,json,tempfile
from pathlib import Path
modpath=Path(__file__).resolve().parents[1]/"tools"/"analyze_v459_rx34_typed_light_pair.py"
spec=importlib.util.spec_from_file_location("v459_analyzer",modpath)
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
def row(frame,serial,entity,light,weight=0,valid=True):
    # kind21 v0..v16 original V451 and v17..v23 V459 extension
    vs=[serial,1,light-1280,entity,entity+0x108,0xe897fc0,0xe80,0,0xdead,
        0x7fb1fb30,1,1,10,7,0,345,0x21919d80,
        63 if valid else 1,light,light,weight,0,0,0x1412eca10]
    assert len(vs)==24
    return [1,1,frame,1234,21]+[f"{v:016x}" for v in vs]
with tempfile.TemporaryDirectory() as td:
    p=Path(td)/"v459_fixture.csv"
    with p.open("w",newline="",encoding="utf8") as f:
        w=csv.writer(f);w.writerow(["seq","qpc","frame","tid","kind"]+[f"v{x:02d}" for x in range(24)])
        w.writerow(row(10,1,0x10000,1310))
        w.writerow(row(10,2,0x20000,1311))
        w.writerow(row(11,3,0x10000,1310))
        w.writerow(row(12,4,0x30000,1310,valid=False))
    a=m.analyze(p)
    assert a["native_entities"]==3
    assert a["summary"]["sample_count"]==4
    assert a["summary"]["typed_pair_reads"]==3
    assert a["summary"]["native_pairs_differ"]==0
    assert a["summary"]["row_endpoint_mismatches"]==0
    assert a["summary"]["entities_with_multiple_rows"]==[]
    assert a["invalid"]["no_typed_read"]==1
    assert a["summary"]["nonfinite_weights"]==0
print("V459 synthetic contract PASS (not runtime proof)")
