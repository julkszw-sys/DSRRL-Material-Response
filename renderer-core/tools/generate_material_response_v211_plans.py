#!/usr/bin/env python3
from __future__ import annotations
import argparse, json
from pathlib import Path

SITES = {
0:((1527,1617),1631),1:((1436,1526),1540),2:((1092,1182),1196),3:((1465,1555),1569),
4:((1374,1464),1478),5:((1034,1124),1138),6:((1254,1344),1358),7:((1163,1253),1267),
8:((819,909),923),9:((1195,1285),1299),10:((1104,1194),1208),11:((764,854),868),
12:((1083,1173),1187),13:((992,1082),1096),14:((648,738),752),15:((1021,1111),1125),
16:((930,1020),1034),17:((590,680),694),18:((997,1087),1101),19:((906,996),1010),
20:((562,652),666),21:((938,1028),1042),22:((847,937),951),23:((507,597),611),
}

def q(s: str) -> str:
    return '"' + s.replace('\\','\\\\').replace('"','\\"') + '"'

def main() -> int:
    ap=argparse.ArgumentParser()
    ap.add_argument('--v29',required=True)
    ap.add_argument('--v210',required=True)
    ap.add_argument('--v211',required=True)
    ap.add_argument('--out',required=True)
    a=ap.parse_args()

    v29=json.loads(Path(a.v29).read_text(encoding='utf-8'))
    v210=json.loads(Path(a.v210).read_text(encoding='utf-8'))['shader']['hosts']
    v211=json.loads(Path(a.v211).read_text(encoding='utf-8'))['hosts']
    stable=sorted((r for r in v29['records'] if not r['lerp']),key=lambda r:r['stable_index'])
    if len(stable)!=24 or len(v210)!=24 or len(v211)!=24:
        raise SystemExit('expected 24 stable HemEnv hosts')

    lines=[
      '#pragma once\n#include <array>\n#include <cstdint>\n#include <string_view>\n\n',
      'namespace dsrrl::operators::material_response::generated {\n',
      'struct word_patch { std::uint32_t word; std::uint32_t old_value; std::uint32_t new_value; };\n',
      'struct v211_plan { std::uint32_t receiver_id; std::uint8_t stable_index; std::uint32_t stock_size; ',
      'std::string_view original_sha256; std::array<std::uint32_t,2> cb_sites; std::uint32_t pow_site; ',
      'std::string_view v29_sha256; std::array<word_patch,2> v210; std::string_view v210_sha256; ',
      'word_patch v211; std::string_view v211_sha256; std::uint32_t replacement_size; };\n',
      'inline constexpr std::array<v211_plan,24> k_v211_plans = {{\n'
    ]

    for r in stable:
        i=r['stable_index']; h210=v210[i]; h211=v211[i]; cb,pow_site=SITES[i]
        p=h210['shex_patch_dwords']
        lines.append(
          '    {'+','.join([
            f'{24+i}u',f'{i}u',f"{r['stock_size']}u",q(r['original_sha256']),
            f'{{{{{cb[0]}u,{cb[1]}u}}}}',f'{pow_site}u',q(r['replacement_sha256']),
            f"{{{{{{{p[0]['word']}u,{int(p[0]['old'],16)}u,{int(p[0]['new'],16)}u}},"
            f"{{{p[1]['word']}u,{int(p[1]['old'],16)}u,{int(p[1]['new'],16)}u}}}}}}",
            q(h210['c101_sha256']),
            f"{{{h211['chain_mul_word']}u,{int(h211['old_instruction_token'],16)}u,{int(h211['new_instruction_token'],16)}u}}",
            q(h211['v211_sha256']),f"{h211['size']}u"
          ])+'},\n'
        )

    lines += ['}};\n\n} // namespace dsrrl::operators::material_response::generated\n']
    Path(a.out).write_text(''.join(lines),encoding='utf-8')
    return 0

if __name__=='__main__':
    raise SystemExit(main())
