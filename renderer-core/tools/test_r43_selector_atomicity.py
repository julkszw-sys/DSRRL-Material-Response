#!/usr/bin/env python3
"""Verify exact R43 stable RC1 -> failure-atomic source publication patch."""
import hashlib
import subprocess
from pathlib import Path

target = Path("renderer-core/src/runtime/pmetal_env_source_runtime.cpp")
base = subprocess.check_output([
    "git", "show",
    "410e4591da9172ffb15e766754b4345286f04691:"+target.as_posix()
])
def sha1(data):
    return hashlib.sha1(b"blob "+str(len(data)).encode("ascii")+b"\0"+data).hexdigest()
assert sha1(base)=="c88cb8f85fac64d7072605d7173f09f984a416cf", "wrong 2.0.2 base"
text=base.decode()
start=text.index("void pmetal_env_source_selector_event(")
end=text.index("bool pmetal_env_source_runtime::latest(",start)
old=text[start:end]
assert old.count("pmetal_producer_state_clear();")==1
assert old.count("pmetal_producer_state_begin(")==1
assert old.count("pmetal_producer_state_publish(")==2
new=target.read_text()
st=new.index("void pmetal_env_source_selector_event(")
en=new.index("bool pmetal_env_source_runtime::latest(",st)
body=new[st:en]
assert body.count("pmetal_producer_state_clear();")==2
assert body.count("pmetal_producer_state_begin(")==2
assert body.count("pmetal_producer_state_publish(")==2
assert body.index("parent_return !=")<body.index("source_b == nullptr")
assert body.index("source_b == nullptr") < body.index("pmetal_producer_state_clear();")
assert body.index("pmetal_producer_state_clear();")<body.index("pmetal_producer_state_begin(")<body.index("pmetal_producer_state_publish(")
first=body.index("pmetal_producer_state_publish(")
second=body.index("pmetal_producer_state_publish(",first+1)
assert body.count("pmetal_producer_state_clear();",first,second)==1
assert body.count("pmetal_producer_state_begin(",first,second)==1
assert body.count("pmetal_producer_state_begin(",second)==0
assert body.index("pmetal_producer_state_clear();",first) < second
assert body.index("pmetal_producer_state_begin(",first) < second
# Pre-patch byte identity: exact substitution, all other 2.0.2 source is unchanged.
base_modified=text
base_modified=base_modified.replace("    pmetal_producer_state_clear();\n\n    if (!g_selector_enabled.load(", "    if (!g_selector_enabled.load(", 1)
old_begin="""    // This exact material now owns the next synchronized source publication.
    // Invalidate any older selector fallback before downstream decode.
    pmetal_producer_state_begin(
        material,
        epoch);

"""
assert base_modified.count(old_begin)==1
base_modified=base_modified.replace(old_begin,"",1)
pattern="        next.serial = epoch;\n        pmetal_producer_state_publish("
assert base_modified.count(pattern)==1
base_modified=base_modified.replace(pattern,
    "        next.serial = epoch;\n        pmetal_producer_state_clear();\n        pmetal_producer_state_begin(material,epoch);\n        pmetal_producer_state_publish(",1)
pattern2="    next.serial = epoch;\n\n    pmetal_producer_state_publish("
assert base_modified.count(pattern2)==1
base_modified=base_modified.replace(pattern2,
    "    next.serial = epoch;\n    pmetal_producer_state_clear();\n    pmetal_producer_state_begin(material,epoch);\n\n    pmetal_producer_state_publish(",1)
assert new==base_modified, "unreviewed source modifications"
print("PASS R43 commit-after-verified-source static proof; exact RC1 source + transaction-only patch")
