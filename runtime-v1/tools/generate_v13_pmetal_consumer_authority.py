#!/usr/bin/env python3
import argparse
import json
from pathlib import Path

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("--input",required=True)
    ap.add_argument("--output",required=True)
    a=ap.parse_args()
    doc=json.loads(Path(a.input).read_text(encoding="utf-8"))
    rows=doc["receivers"]
    if len(rows)!=3:
        raise SystemExit(f"expected 3 P_Metal receivers, got {len(rows)}")
    if [r["receiver_id"] for r in rows] != [33,34,35]:
        raise SystemExit("receiver authority must be exact 33/34/35")
    lines=[
        "#pragma once",
        "#include <array>",
        "#include <cstddef>",
        "#include <string_view>",
        "",
        "namespace dsrrl::runtime::mr::v13_authority {",
        "",
        "struct entry {",
        "    std::string_view input_v211_sha256;",
        "    std::string_view output_v13_sha256;",
        "    std::size_t t12_word;",
        "    std::size_t merge_word;",
        "};",
        "",
        "inline constexpr std::array<entry,3> k_pmetal = {{",
    ]
    for r in rows:
        lines += [
            f'    {{"{r["input_v211_sha256"]}",',
            f'     "{r["output_v13_sha256"]}",',
            f'     {r["t12_word"]}u,{r["merge_word"]}u}},',
        ]
    lines += [
        "}};",
        "",
        "constexpr const entry *find(std::string_view sha) noexcept",
        "{",
        "    for(const auto &e:k_pmetal)",
        "        if(e.input_v211_sha256==sha)",
        "            return &e;",
        "    return nullptr;",
        "}",
        "",
        "} // namespace dsrrl::runtime::mr::v13_authority",
        "",
    ]
    Path(a.output).write_text("\n".join(lines),encoding="utf-8")

if __name__=="__main__":
    main()
