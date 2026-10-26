from pathlib import Path
r=Path("renderer-core")
s=(r/"src/runtime/pmetal_envspec_draw_runtime.cpp").read_text()
h=(r/"include/dsrrl/runtime/v387_stable_envdiffuse_cut.hpp").read_text()
assert s.count('v387_stable_cut(material,decision,source,prepared,true)')==1
assert s.count('v387_stable_cut(material,decision,source,prepared,false)')==1
assert "if (family == pmetal_envspec_receiver_family::stable_hemenv)" in s
assert "[DSRRL V387 STABLE CUT]" in h
assert "flver_sha=%s" in h
for x in ("s.bank_signature_a","s.row_id_a","s.serial","s.envdiffuse_a[0]",
          "p.envdiffuse_resources.probe_a","prepared.envdiffuse_resources.ready",
          "GetTickCount64()"):
    assert x in (h+s),x
assert "request=READY pixel=OPEN" in h
assert "STOCK_DSR" not in h
print("V387 STABLE source to CB12 and t11 read-only RE witness PASS")
