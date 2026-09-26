#!/usr/bin/env python3
from __future__ import annotations
import argparse, csv
from pathlib import Path

def q(s: str) -> str:
    return '"' + s.replace('\\','\\\\').replace('"','\\"') + '"'

def main() -> int:
    ap=argparse.ArgumentParser()
    ap.add_argument('--input',required=True)
    ap.add_argument('--output',required=True)
    a=ap.parse_args()

    with Path(a.input).open('r',encoding='utf-8',newline='') as f:
        rows=list(csv.DictReader(f,delimiter='\t'))

    if len(rows)!=24:
        raise SystemExit(f'expected 24 HemEnvLerp rows, got {len(rows)}')
    for i,r in enumerate(rows):
        if int(r['pair_index'])!=i:
            raise SystemExit(f'non-contiguous pair_index at {i}')
        for k in ('original_sha256','v29_sha256','v210_sha256','v211_sha256'):
            if len(r[k])!=64:
                raise SystemExit(f'bad sha {k} row {i}')

    out=[
'#pragma once\n#include <array>\n#include <cstdint>\n#include <string_view>\n\n',
'namespace dsrrl::operators::material_response::generated {\n\n',
'struct hemenvlerp_v211_plan {\n',
'    std::uint8_t pair_index;\n',
'    std::string_view label;\n',
'    std::uint32_t stock_size;\n',
'    std::string_view stock_sha256;\n',
'    std::array<std::uint32_t,2> cb_sites;\n',
'    std::uint32_t pow_site;\n',
'    std::string_view v29_sha256;\n',
'    std::array<std::uint32_t,2> v210_sites;\n',
'    std::string_view v210_sha256;\n',
'    std::uint32_t v211_word;\n',
'    std::string_view v211_sha256;\n',
'    std::uint32_t replacement_size;\n',
'};\n\n',
'inline constexpr std::array<hemenvlerp_v211_plan,24> k_hemenvlerp_v211_plans = {{\n'
    ]
    for r in rows:
        out.append(
            '    {'+
            f"{int(r['pair_index'])}u,{q(r['label'])},{int(r['stock_size'])}u,{q(r['original_sha256'])},"+
            f"{{{{{int(r['cb0'])}u,{int(r['cb1'])}u}}}},{int(r['pow'])}u,{q(r['v29_sha256'])},"+
            f"{{{{{int(r['v210_w0'])}u,{int(r['v210_w1'])}u}}}},{q(r['v210_sha256'])},"+
            f"{int(r['v211_word'])}u,{q(r['v211_sha256'])},{int(r['replacement_size'])}u"+
            '},\n'
        )
    out += ['}};\n\n} // namespace dsrrl::operators::material_response::generated\n']
    p=Path(a.output); p.parent.mkdir(parents=True,exist_ok=True)
    p.write_text(''.join(out),encoding='utf-8')
    return 0

if __name__=='__main__':
    raise SystemExit(main())
