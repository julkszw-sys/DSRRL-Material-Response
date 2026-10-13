#!/usr/bin/env python3
"""Build-time exact authored PTDE EnvSpec RGBxMultiplier bank/row carrier.

Extracted from original PTDE DrawParam 20 LightBank PARAMs, 1246 rows.
Each row is four s16 at offset 72..79 in 92-byte LIGHT_BANK:
envSpc_colR/G/B/A (Paramdex DS1 LightBank paramdef v4).
Embedded compressed donor is a DATA snapshot, NOT source code to execute.
PTDE source archive SHA256 aaa462dd374ad641a3f3b54ecc441e4bce1a408e5fca4611445a07dc72d06d38.
PTDE rows 1246; vanilla DSR 161 distinct RGBM quartets, 1085 identical.
"""
import argparse,base64,csv,hashlib,struct,zlib
from pathlib import Path

K_RAW_SHA="4626e8c39940f756bbb68409421a4e0d7a4e7bc1ba3835aeaea63418746b2426"
K_PTDE_ROWS=1246
K_SOURCE_B64="""eNrtWi1sGzEUftduJGTSdKQD0aSRgI4c6UBwQFHJpClgI8YlIwEbKS4IOhIyXDJypHQjBgsZKQkpqSaFBGwkJPOd/S6+d37xXXJJG7WKqk9+/nt+37P9/K4LWKjfcbDIUEIR5waxfmDKkdXuFsZwZ8p/S+OMVG15XIoXBi+Z+pi0E0QuCLZJOSKI66HjSc8420LJlCVTb+NY/Z4x/KEc18utZ13+U7nNP8Bm/F/Baj2Eh3/UM2LsGXnsKT39znbsD7viX8AEZhZGhE/EDmkvFQMTx7wDgjhvSMpcfejxT+HxB+zfJXrhOFV5iIl+3TV5G3iQrrMqVl0H7dcqzV+Nf7H3/OvzaM6Mm5D2wPjN0p5T+JfJEfG80GWqL+R6uuujvF7rieu5I/rFlv1mahQ8Z+eknt6bWP7GzHuk/iZqFurPp8FmflodR2ol/D4CEifEud1HcFPh3Bww58Gm/Nt6jBz3Et1Pw6DIP7UD5RPlCZEnHr9GpP6wPf52jevxj/d4H9zxQgwnGY+C4TFk5MPAHW8ifizVv1fz3JbuFS6OoYjxNurznGkXQrXzU9S87yJPvE7Pc2zfIzz4EMf9EayOw9BuQPq3CPr5d8ub51/jjHmf1OVfVIzv68Y70Zr8c0j1app/Oi7lf3nuXsO12oHn2b2H5/eoYKepKqeYyt+ZFl1ip+L9IBXSuEiPG6oaoawWm/HSfilCaR04zyhrKXJcqJPpJNfH9f4Fx71yCZvx33S8y8Xl3Du0qLfM7PFJ2eH1ocZhoHFi5G1YHS/b8eDC8DBZEa8hH3ScNuFXxy8puuPiY6PncVA13kX9lnq6+PpzWM+unF32/b1Ttz93rtV57/xW0bSdF0nLvbXfO4m5H5OCvGXK5f0uzLzCxO9JAV+YGc9MxH+Ujzcw+gwKPG+D/zslvbCwKf71eEkhnth//otI763Y8S6/UtK+0koqW9Dx6DvflidKzzhbQ5Ldkym2GuIf/VhY7zSNSz/W846VvSScEj908Yd2XuVPIRN/xtZ6Xbw+nHxH0Z+PKsbrvnxXm/GLNsHHdv5T/sW981/vfIKK/N/3/hcmg9Un/k3jrS7shn9J8p09g2dEXpX/qLSvJlkuRnj84Snfva18t7a/JDz4+cd9VY//p+8dD/t7B//+c3+3RvmHA+5dOoZbK4+O/nTB5JElaYd2OM/1vskipGU8o/O5Lw9Wr78q/1X9ieq7v/d/P7NNnPOhy8OgGf5D8h2F8s/ZZRi4+Q8ZXpva/4+P/9X9ad4S8yfSEz9Jz3q4cWi/2MorTq18I5dfBvisfl/ZfPSckfeY/D8w533ksOdUnXQtz3ext4xfdBzvlJmKBV+R+i9BKv+Z3a22/A1oeQRaj++knv4/Q4fsK9TvV+DOqz8E/l3fR3bFPzD8S1jvvFh3/wM0e/8Llv//wbEfhw=="""

def create(layout:Path,output:Path):
    raw=zlib.decompress(base64.b64decode(K_SOURCE_B64))
    if len(raw)!=K_PTDE_ROWS*8 or hashlib.sha256(raw).hexdigest()!=K_RAW_SHA:
        raise ValueError("V253 PTDE donor byte hash / length mismatch")
    table=list(csv.DictReader((ln for ln in layout.read_text(encoding="utf-8").splitlines() if ln.strip() and not ln.lstrip().startswith("#")),delimiter="\t"))
    if len(table)!=20:
        raise ValueError("V253 expected 20 exact V13 LightBank identities")
    exp_counts=[64,64,64,30,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64]
    banks=[]
    offset=0
    for i,b in enumerate(table):
        n=int(b["donor_count"])
        if int(b["bank_index"])!=i or n!=exp_counts[i] or not b["bank_name"].endswith("_LightBank.param") or len(b["v13_signature"])!=16:
            raise ValueError("V253 semantic bank mapping / count mismatch")
        banks.append((int(b["v13_signature"],16),offset,n))
        offset+=n
    if offset!=K_PTDE_ROWS:raise ValueError("V253 semantic row_count mismatch")
    values=list(struct.iter_unpack("<4H",raw))
    if len(values)!=K_PTDE_ROWS:raise ValueError("V253 record count mismatch")
    if any(any(ch>255 for ch in r[:3]) or r[3]>10000 for r in values):
        raise ValueError("V253 PTDE color/multiplier out of parameter bounds")
    lines=["#pragma once","#include <array>","#include <cstdint>",
        "// Exact PTDE_LIGHT_BANK envSpc_colR/G/B/A, no DSR offsets/gains.",
        "namespace dsrrl::runtime::v253_ptde_envspec_rgbm {",
        "struct row { std::uint16_t r,g,b,m; };",
        "struct bank { std::uint64_t signature; std::uint32_t first,count; };",
        "inline constexpr std::array<bank,20> banks{{"]
    for sig,first,count in banks:
        lines.append(f"    {{0x{sig:016x}ULL,{first}u,{count}u}},")
    lines.extend(["}};","inline constexpr std::array<row,1246> rows{{"])
    for r,g,b,m in values:
        lines.append(f"    {{{r}u,{g}u,{b}u,{m}u}},")
    lines.extend(["}};",
        "inline bool lookup(std::uint64_t signature,std::uint32_t row_id,std::array<float,3> &rgb) noexcept {",
        "  for (const auto &b : banks) {",
        "    if (b.signature != signature) continue;",
        "    if (row_id >= b.count) return false;",
        "    const auto &v=rows[b.first+row_id];",
        "    const float k=static_cast<float>(v.m)*(0.01f/255.0f);",
        "    rgb={{v.r*k,v.g*k,v.b*k}};",
        "    return true;",
        "  }",
        "  return false;",
        "}",
        "} // namespace dsrrl::runtime::v253_ptde_envspec_rgbm",
        ""])
    output.parent.mkdir(parents=True,exist_ok=True)
    output.write_text("\n".join(lines),encoding="utf-8")
    print("V253_EXACT_PTDE_ENVSPEC_RGBM_BANK20_ROWS1246_SHA_"+K_RAW_SHA+" PASS")

if __name__=="__main__":
    p=argparse.ArgumentParser()
    p.add_argument("--layout",default="renderer-core/data/provenance/pmetal_env_source_layout_v1.tsv")
    p.add_argument("--output",default="renderer-core/include/dsrrl/runtime/generated_v253_ptde_envspec_rgbm.hpp")
    a=p.parse_args()
    create(Path(a.layout),Path(a.output))
