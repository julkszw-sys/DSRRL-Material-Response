#!/usr/bin/env python3
"""Source-exact direct FINAL v2.0.2 hybrid, no inherited postrelease commits."""
from pathlib import Path
import hashlib, re, zlib

root=Path("renderer-core")
target=root/"src/runtime/pmetal_env_source_runtime.cpp"
candidate=root/"data/provenance/r43_zero_selector_hybrid_source.cpp"
compressed=root/"data/provenance/dsr_only_lightbank_rgbm_v1.z"
header=root/"include/dsrrl/runtime/dsr_only_lightbank_rgbm_v1.hpp"
def gitsha(b):
    return hashlib.sha1(("blob %d\0"%len(b)).encode()+b).hexdigest()
original=target.read_bytes().replace(b'\r\n',b'\n')
assert gitsha(original)=="a6904844c97c712027ad79bbcd136066952a7116", ("NOT FINAL v2.0.2",gitsha(original))
source=candidate.read_bytes().replace(b'\r\n',b'\n')
assert gitsha(source)=="98a4d3576d10cd7387ec52e24daecf7c70602947", "Candidate corrupted"
s=source.decode()
regex=r"(?m)^([ \t]*)(g_hook_(?:publish|consume|single_seen|blend_seen)\.fetch_add\(\n[ \t]*1u,\n[ \t]*std::memory_order_relaxed\);)"
s,count=re.subn(regex,lambda m:m[1]+"#if !defined(DSRRL_RELEASE_CLEANUP)\n"+m[2]+"\n"+m[1]+"#endif",s)
assert count==9, "Production release cleanup rebase incomplete"
assert "latest_hook_source(out)" not in s and "[DSRRL PMETAL ZERO RE]" in s

def consumer_block(text):
    start = text.index("bool pmetal_env_source_runtime::latest(")
    end = text.index("pmetal_env_source_runtime_telemetry\\n", start)
    return text[start:end]
baseline_consumer = consumer_block(original.decode("utf-8"))
candidate_consumer = consumer_block(s)
assert "if (latest_hook_source(out))" in baseline_consumer
assert "if (latest_hook_source(out))" not in candidate_consumer
assert s.count(candidate_consumer) == 1
s = s.replace(candidate_consumer, baseline_consumer, 1)
assert consumer_block(s) == baseline_consumer
assert "exact_dsr_only_endpoint(" in s
data=zlib.decompress(compressed.read_bytes())
assert hashlib.sha256(data).hexdigest()=="c5a3b3731c1dd95601458898f12bb9e7b4711b0dafa0d0a1a59929cd58fb93c2"
assert b"std::array<donor,195>" in data
header.write_bytes(data)
target.write_text(s,encoding="utf-8",newline="\n")
print("PASS original FINAL 2.0.2 -> zero-selector & 195 original DSR-only P_Metal donors")
