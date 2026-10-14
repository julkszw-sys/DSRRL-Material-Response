#!/usr/bin/env python3
"""V299 PROTECT regression. Compare exact R43 semantic function bodies.
Source-only protection; source fingerprints do NOT assert runtime pixel parity.
"""
from pathlib import Path

HERE = Path(__file__).resolve().parents[2]
FILES = (
    ("src/runtime/pmetal_env_source_runtime.cpp",
     "bool latest_hook_source(", "9e0df4545bf2dff0"),
    ("src/runtime/pmetal_env_source_runtime.cpp",
     "bool pmetal_env_source_runtime::latest(", "7979965fe2c303d4"),
    ("src/runtime/pmetal_producer_state.cpp",
     "bool pmetal_producer_state_latest(", "fa9c84e673b2e5a0"),
)

def fingerprint(s):
    h=0xcbf29ce484222325
    for b in s.encode("utf-8"):
        h=((h^b)*0x100000001b3) & 0xffffffffffffffff
    return f"{h:016x}"

def function_block(text, signature):
    pos=text.find(signature)
    if pos<0 or text.find(signature,pos+1)>=0:
        raise AssertionError("Protected function missing or ambiguous")
    at=text.find("{",pos)
    if at<0:raise AssertionError("Missing body")
    depth=0
    for i in range(at,len(text)):
        if text[i]=="{":depth+=1
        elif text[i]=="}":
            depth-=1
            if depth==0:return text[pos:i+1]
    raise AssertionError("Unbalanced protected body")

def main():
    for path,signature,want in FILES:
        found=fingerprint(function_block((HERE/path).read_text(),signature))
        if found!=want:raise AssertionError(f"PROTECT violated: {signature}: {found}")
    runtime=(HERE/"src/runtime/pmetal_env_source_runtime.cpp").read_text()
    if runtime.count("if (latest_hook_source(out))") != 1:
        raise AssertionError("Unchanged fallback call required")
    if runtime.count("pmetal_donor_provenance::classify(")!=1:
        raise AssertionError("Donor provenance must be row-local")
    print("V299_LATEST_HOOK_SOURCE_PROTECT_R43_EXACT_BODY_PASS")
if __name__=="__main__":main()
