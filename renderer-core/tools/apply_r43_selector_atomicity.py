#!/usr/bin/env python3
"""Materialize exactly one audited R43 RC1 selector transaction patch."""
import hashlib
from pathlib import Path
p=Path("renderer-core/src/runtime/pmetal_env_source_runtime.cpp")
data=p.read_bytes()
def blobhash(b):return hashlib.sha1(b"blob "+str(len(b)).encode()+b"\0"+b).hexdigest()
assert blobhash(data)=="c88cb8f85fac64d7072605d7173f09f984a416cf", "source not stable 2.0.2 R43"
s=data.decode()
edits=[
("    pmetal_producer_state_clear();\n\n    if (!g_selector_enabled.load(", "    if (!g_selector_enabled.load("),
("""    // This exact material now owns the next synchronized source publication.
    // Invalidate any older selector fallback before downstream decode.
    pmetal_producer_state_begin(
        material,
        epoch);

""",""),
("        next.serial = epoch;\n        pmetal_producer_state_publish(",
 "        next.serial = epoch;\n        pmetal_producer_state_clear();\n        pmetal_producer_state_begin(material,epoch);\n        pmetal_producer_state_publish("),
("    next.serial = epoch;\n\n    pmetal_producer_state_publish(",
 "    next.serial = epoch;\n    pmetal_producer_state_clear();\n    pmetal_producer_state_begin(material,epoch);\n\n    pmetal_producer_state_publish(")
]
for a,b in edits:
    assert s.count(a)==1, "unexpected selector source context"
    s=s.replace(a,b,1)
p.write_bytes(s.encode())
print("R43 selector transaction source exact patch PASS")
