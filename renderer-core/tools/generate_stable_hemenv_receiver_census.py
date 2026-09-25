#!/usr/bin/env python3
from __future__ import annotations
import argparse, json
from pathlib import Path

EXPECTED_EVIDENCE="59141f89-cc1d-4427-b8d2-5f02a5cdeb45"

def digest_init(hex_text:str)->str:
    raw=bytes.fromhex(hex_text)
    if len(raw)!=32: raise ValueError("bad sha256")
    return "{{"+",".join(f"0x{x:02x}u" for x in raw)+"}}"

def render(doc:dict)->str:
    if doc.get("schema")!="dsrrl.stable_hemenv_receiver_census.v1":
        raise ValueError("schema mismatch")
    if doc.get("status")!="CONFIRMED" or doc.get("evidence_id")!=EXPECTED_EVIDENCE:
        raise ValueError("authority mismatch")
    entries=doc.get("entries",[])
    if len(entries)!=24:
        raise ValueError(f"expected 24 receivers, got {len(entries)}")
    ids=[int(e["receiver_id"]) for e in entries]
    if ids!=list(range(24,48)):
        raise ValueError("receiver ids must be contiguous 24..47")
    seen=set()
    out=[
        "#pragma once\n",
        "#include <array>\n#include <cstddef>\n#include <cstdint>\n\n",
        "namespace dsrrl::runtime::generated {\n",
        "struct stable_hemenv_receiver_record {\n",
        "    std::uint32_t receiver_id;\n",
        "    std::uint32_t shader_index;\n",
        "    std::uint32_t byte_size;\n",
        "    std::array<std::uint8_t,32> code_sha256;\n",
        "};\n",
        "inline constexpr std::array<stable_hemenv_receiver_record,24> "
        "k_stable_hemenv_receivers_v1 = {{\n",
    ]
    for e in entries:
        sha=str(e["code_sha256"]).lower()
        if sha in seen: raise ValueError("duplicate receiver shader sha")
        seen.add(sha)
        if int(e["byte_size"])<=0: raise ValueError("invalid byte_size")
        out.append(
            "    {"
            f'{int(e["receiver_id"])}u,{int(e["shader_index"])}u,{int(e["byte_size"])}u,'
            + digest_init(sha) + "},\n"
        )
    out.extend([
        "}};\n",
        "constexpr bool stable_hemenv_candidate_size(std::size_t byte_size) noexcept{"
        "for(const auto&r:k_stable_hemenv_receivers_v1)if(r.byte_size==byte_size)return true;"
        "return false;}\n",
        "constexpr std::uint32_t stable_hemenv_receiver_id("
        "const std::array<std::uint8_t,32>&sha,std::size_t byte_size) noexcept{"
        "std::uint32_t hit=0u;"
        "for(const auto&r:k_stable_hemenv_receivers_v1){"
        "if(r.byte_size!=byte_size||r.code_sha256!=sha)continue;"
        "if(hit!=0u)return 0u;hit=r.receiver_id;}return hit;}\n",
        "} // namespace dsrrl::runtime::generated\n",
    ])
    return "".join(out)

def main()->int:
    ap=argparse.ArgumentParser()
    ap.add_argument("source",type=Path)
    ap.add_argument("output",type=Path)
    args=ap.parse_args()
    doc=json.loads(args.source.read_text(encoding="utf-8"))
    text=render(doc)
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text(text,encoding="utf-8",newline="\n")
    return 0

if __name__=="__main__":
    raise SystemExit(main())
