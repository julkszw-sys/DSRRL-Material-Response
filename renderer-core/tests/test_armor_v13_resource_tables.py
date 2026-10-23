#!/usr/bin/env python3
"""Independent source-level validation of the PTDE BASE armor v13 resource delta.

Never mistake a source route census for DSR receiver-runtime or pixel PASS.
Requires materialize_v230_armor_v13_exact_supplement.py to have run first.
"""
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
INC = ROOT / "include" / "dsrrl" / "runtime"
REPORT = ROOT / "data" / "census" / "armor_v13_20261023_report.json"
HEADER = INC / "generated_armor_v13_base_exact.hpp"
RUNTIME = ROOT / "src" / "runtime" / "material_resource_draw_runtime.cpp"

def fnv(s):
    h=0xcbf29ce484222325
    for c in s.casefold().encode("utf-8"):
        h=((h^c)*0x100000001b3)&0xffffffffffffffff
    return h

def decode_triplet(head, symbol, arity):
    # Only read array initializer, not its other helper functions.
    regex = re.compile(rf"inline constexpr std::array<(?:pair|triple),\d+>\s+{symbol}\s*=\s*\{{\{{\s*(.*?)\s*\}}\}};", re.S)
    match=regex.search(head)
    assert match, f"missing C++ array {symbol}"
    r = re.compile(r"\{\s*"+r"\s*,\s*".join([r"0x([0-9a-f]{16})ull"]*arity)+r"\s*\}",re.I)
    return [tuple(int(x,16) for x in groups) for groups in r.findall(match.group(1))]

def decode_v12(path, arity):
    s=path.read_text("utf-8")
    pat=r"\{\s*"+r"\s*,\s*".join([r"0x([0-9a-f]{16})ull"]*arity)+r"\s*\}"
    return {tuple(int(x,16) for x in row) for row in re.findall(pat,s,re.I)}

def main():
    report=json.loads(REPORT.read_text("utf-8"))
    head=HEADER.read_text("utf-8")
    runtime=RUNTIME.read_text("utf-8")
    assert report["existing_v12_pairs"]==528
    assert report["existing_v12_normal_triples"]==557
    assert report["ptde_spec_unique"]==548
    assert report["missing_pairs_added"]==34
    assert report["missing_triples_added"]==1
    pairs=decode_triplet(head,"p",2)
    triples=decode_triplet(head,"t",3)
    assert len(pairs)==len(set(pairs))==34, f"duplicate/missing pairs {len(pairs)}"
    assert len(triples)==len(set(triples))==1, f"duplicate/missing triples {len(triples)}"
    oldp=decode_v12(INC/"generated_diffuse_routes_v12.hpp",2)
    oldt=decode_v12(INC/"generated_normal_routes_v12.hpp",3)
    assert len(oldp)==528 and len(oldt)==557
    assert not (set(pairs)&oldp), "v13 duplicates an existing v12 pair"
    assert not (set(triples)&oldt), "v13 duplicates a v12 normal triple"
    expected_pair=(fnv("hd_m_9360_s"),fnv("hd_m_9360"))
    expected_triple=(fnv("hd_m_9360"),fnv("hd_m_9360_s"),fnv("hd_m_9360_n"))
    assert expected_pair in pairs and expected_triple in triples
    assert report["ambiguous_source_names"] == ["lg_f_9400_lg_m_9400_s"]
    # The prepared draw requires a source-complete exact DSR owner, RX and native t0/1/2.
    for token in [
        "has_exact_flver_material_ownership(query)",
        "receiver_id >= 24u && receiver_id <= 35u",
        "armor_v13::pair_ok(h1,h0)",
        "armor_v13::triple_ok(h0,h1,h2)",
        "armor_v13::diffuse_name(hash)",
        "armor_v13::normal_name(hash)",
        "armor_v13::diffuse_name(logical_hash)",
        "armor_v13::normal_name(logical_hash)",
    ]:
        assert token in runtime, "resource route gate/loader missing "+token
    assert report.get("routing_scope","").find("runtime and pixels OPEN")!=-1
    print("ARMOR_V13_SOURCE_AND_RUNTIME_GATES_STATIC_PASS",json.dumps({
        "v12_pairs":len(oldp),"v13_added_pairs":len(pairs),
        "v12_triples":len(oldt),"v13_added_triples":len(triples),
        "new_helmet_pair":expected_pair,"new_helmet_triple":expected_triple,
        "ambiguous_excluded":report["ambiguous_source_names"]
    }))
    print("WARNING: DSR live t0/t1/t2, receiver-hit and PTDE pixel equivalence NOT TESTED")

if __name__=="__main__":
    main()
