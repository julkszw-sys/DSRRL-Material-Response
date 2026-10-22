#!/usr/bin/env python3
"""PR485 owner log 055503: retain bounded GI origin first-consumer evidence.

Apply after V359 materializers and before the V230 release-info cleanup.
Does NOT assign unknown t12/t14 SHA, modify resources, or patch consumers.
"""
from pathlib import Path

root=Path(__file__).resolve().parents[1]
p=root/"src/runtime/pmetal_envspec_draw_runtime.cpp"
s=p.read_text("utf8")

def replace_single_after(marker, needle, repl, max_delta):
    global s
    start=s.index(marker)
    hit=s.find(needle,start)
    if hit<0 or hit-start>max_delta:
        raise RuntimeError("missing exact bounded logger after "+marker)
    if s.find(needle,hit+len(needle),start+max_delta)>=0:
        raise RuntimeError("ambiguous log after "+marker)
    s=s[:hit]+repl+s[hit+len(needle):]

log="reshade::log::message(reshade::log::level::info,line);"
warn="reshade::log::message(reshade::log::level::warning,line);"
# V354 logs once per receiver/slot (bitmask), not each draw.
replace_single_after('"[DSRRL V354 PROBE REJECT]',log,warn,1650)
# V358 is only called within the same V354 first-hit bucket.
replace_single_after('"[DSRRL V358 PROBE ORIGIN]',log,warn,1550)
# The V357 source-name trace is called only on the bounded V354 reject.
begin=s.index("void v357_trace_rejected_srv(")
end=s.index("\n#endif",begin)
block=s[begin:end]
if block.count(log)!=2:
    raise RuntimeError("V357 source trace cardinality drift")
block=block.replace(log,
    "if (binding == 12u || binding == 14u)\n"
    "        reshade::log::message(reshade::log::level::warning,line);")
s=s[:begin]+block+s[end:]
p.write_text(s,"utf8")

stable=(root/"src/operators/env_spec/pmetal_rgba_materializer.cpp").read_text("utf8")
assert "mr.family == material_response::diffuse_v1_family::hemenvlerp" in stable
assert "outcome.receiver_id = mr.receiver_id;" in stable
print("V230_GI_ORIGIN_BOUNDED_T12_T14_AND_STABLE_FAMILY_GUARD_PASS")
print("Stable HemEnv false diffuse failure Lerp now pass_not_candidate")
print("GI probe aliases unchanged: no unknown BC6H SHA admitted")
