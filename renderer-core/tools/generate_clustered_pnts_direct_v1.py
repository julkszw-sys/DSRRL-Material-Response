#!/usr/bin/env python3
import argparse
import json
from pathlib import Path

def u32(v):
    return f"{int(v)}u"

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("--input",required=True)
    ap.add_argument("--output",required=True)
    ns=ap.parse_args()
    data=json.loads(Path(ns.input).read_text(encoding="utf-8"))
    if data.get("schema")!="DSRRL_CLUSTERED_PNTS_DIRECT_PTDE_STOCK_JOURNAL_V1":
        raise SystemExit("unexpected clustered PntS journal schema")
    entries=data.get("entries",[])
    if len(entries)!=36:
        raise SystemExit(f"expected 36 exact clustered PntS plans, got {len(entries)}")

    tokens=[]
    ops=[]
    plans=[]
    for e in entries:
        first=len(ops)
        for op in e["ops"]:
            old=list(map(int,op["old_tokens"]))
            new=list(map(int,op["new_tokens"]))
            old_off=len(tokens); tokens.extend(old)
            new_off=len(tokens); tokens.extend(new)
            ops.append((
                int(op["start"]),int(op["end"]),
                old_off,len(old),new_off,len(new)
            ))
        material=str(e.get("material_class",""))
        capture=str(e.get("capture_class",""))
        plans.append((
            e["original_sha256"],e["replacement_sha256"],
            int(e["stock_size"]),int(e["replacement_size"]),
            first,len(ops)-first,
            material=="Spc",
            capture=="SPC_MUL_BLEND",
            int(e["shader_index"])
        ))

    out=[]
    out += ["#pragma once","","#include <array>","#include <cstddef>","#include <cstdint>","","namespace dsrrl::operators::point_light::generated {",""]
    out += ["struct clustered_pnts_journal_op_v1 {","    std::uint32_t start;","    std::uint32_t end;","    std::uint32_t old_offset;","    std::uint32_t old_count;","    std::uint32_t new_offset;","    std::uint32_t new_count;","};",""]
    out += ["struct clustered_pnts_plan_v1 {","    const char *original_sha256;","    const char *replacement_sha256;","    std::uint32_t stock_size;","    std::uint32_t replacement_size;","    std::uint32_t first_op;","    std::uint32_t op_count;","    bool spc;","    bool blended_material;","    std::uint32_t representative_shader_index;","};",""]
    out += [f"inline constexpr std::array<std::uint32_t,{len(tokens)}> k_clustered_pnts_journal_tokens_v1 = {{{{"]
    for i in range(0,len(tokens),8):
        out.append("    "+",".join(u32(x) for x in tokens[i:i+8])+",")
    out += ["}};",""]
    out += [f"inline constexpr std::array<clustered_pnts_journal_op_v1,{len(ops)}> k_clustered_pnts_journal_ops_v1 = {{{{"]
    for x in ops:
        out.append("    {"+",".join(u32(v) for v in x)+"},")
    out += ["}};",""]
    out += [f"inline constexpr std::array<clustered_pnts_plan_v1,{len(plans)}> k_clustered_pnts_plans_v1 = {{{{"]
    for p in plans:
        out.append(
            '    {"'+p[0]+'","'+p[1]+'",'+u32(p[2])+','+u32(p[3])+','+
            u32(p[4])+','+u32(p[5])+','+
            ("true" if p[6] else "false")+','+
            ("true" if p[7] else "false")+','+u32(p[8])+'},')
    out += ["}};","","} // namespace dsrrl::operators::point_light::generated",""]
    Path(ns.output).parent.mkdir(parents=True,exist_ok=True)
    Path(ns.output).write_text("\n".join(out),encoding="utf-8")

if __name__=="__main__":
    main()
