#!/usr/bin/env python3
from __future__ import annotations
import argparse, json
from pathlib import Path

def digest_array(hex_digest: str) -> str:
    raw=bytes.fromhex(hex_digest)
    if len(raw)!=32:
        raise SystemExit("bad SHA-256")
    return "{{"+",".join(f"0x{x:02x}u" for x in raw)+"}}"

def bits3(values):
    if len(values)!=3:
        raise SystemExit("expected vec3 bits")
    return "{{"+",".join(f"0x{int(x,16):08x}u" for x in values)+"}}"

def main()->int:
    ap=argparse.ArgumentParser()
    ap.add_argument("--input",type=Path,required=True)
    ap.add_argument("--output",type=Path,required=True)
    ns=ap.parse_args()
    doc=json.loads(ns.input.read_text(encoding="utf-8"))
    if doc.get("schema")!="DSRRL_POINTLIGHT_MATERIAL_AUTHORITY_V1":
        raise SystemExit("unexpected schema")
    active=doc.get("active_records",[])
    if len(active)!=205:
        raise SystemExit(f"expected 205 active records, got {len(active)}")
    spc=sum(1 for r in active if r.get("material_mode")=="SPC")
    nospc=sum(1 for r in active if r.get("material_mode")=="NOSPC")
    rgb_c101=sum(
        1 for r in active
        if r.get("material_mode")=="SPC"
        and len(set(r.get("c101_f32_bits",[])))>1
    )
    if (spc,nospc,rgb_c101)!=(180,25,12):
        raise SystemExit(
            "PointLight authority partition drift: "
            f"spc={spc} nospc={nospc} rgb_c101={rgb_c101}"
        )

    seen={}
    rows=[]
    for ordinal,r in enumerate(active):
        if r.get("authority")!="POINTLIGHT_MATERIAL_CONSTANTS_ONLY":
            raise SystemExit("authority widening")
        if r.get("dsr_spx")!=r.get("ptde_spx"):
            raise SystemExit("non exact-SPX active row")
        sem=int(r["semantic_name_hash_fnv1a_utf8"],16)
        sha=r["dsr_mtd_sha256"].lower()
        previous=seen.get(sem)
        if previous is not None and previous!=sha:
            raise SystemExit(f"ambiguous semantic 0x{sem:016x}")
        seen[sem]=sha
        mode=r["material_mode"]
        if mode not in ("SPC","NOSPC"):
            raise SystemExit("bad material mode")
        c101=r["c101_f32_bits"]
        rows.append((sem,sha,mode,r["c100_f32_bits"],c101,r["c102_f32_bits"],len(set(c101))==1,ordinal))

    rows.sort(key=lambda x:(x[0],x[1]))
    out=[
        "#pragma once\n",
        "#include <array>\n#include <cstddef>\n#include <cstdint>\n\n",
        "namespace dsrrl::operators::point_light::generated {\n",
        "enum class pointlight_material_mode : std::uint8_t { nospc=0, spc=1 };\n",
        "struct pointlight_material_authority_record {\n",
        " std::uint64_t semantic_name_hash; std::array<std::uint8_t,32> raw_mtd_sha256;\n",
        " pointlight_material_mode mode; std::array<std::uint32_t,3> c100_bits;\n",
        " std::array<std::uint32_t,3> c101_bits; std::uint32_t c102_bits;\n",
        " bool c101_scalar; std::uint32_t source_ordinal;\n};\n",
        f"inline constexpr std::array<pointlight_material_authority_record,{len(rows)}u> k_pointlight_material_authority_v1={{{{\n"
    ]
    for sem,sha,mode,c100,c101,c102,scalar,ordinal in rows:
        m="pointlight_material_mode::spc" if mode=="SPC" else "pointlight_material_mode::nospc"
        out.append(
            f" {{0x{sem:016x}ull,{digest_array(sha)},{m},{bits3(c100)},{bits3(c101)},"
            f"0x{int(c102,16):08x}u,{str(scalar).lower()},{ordinal}u}},\n")
    out += ["}};\n","} // namespace dsrrl::operators::point_light::generated\n"]
    ns.output.parent.mkdir(parents=True,exist_ok=True)
    ns.output.write_text("".join(out),encoding="utf-8",newline="\n")
    print(f"POINTLIGHT_MATERIAL_AUTHORITY_HEADER_PASS records={len(rows)} spc={spc} nospc={nospc} rgb_c101={rgb_c101}")
    return 0

if __name__=="__main__":
    raise SystemExit(main())
