#!/usr/bin/env python3
from __future__ import annotations
import argparse, json
from pathlib import Path

def q(s: str) -> str:
    return '"' + s.replace('\\','\\\\').replace('"','\\"') + '"'

def main() -> int:
    ap=argparse.ArgumentParser()
    ap.add_argument('--v29',required=True)
    ap.add_argument('--out',required=True)
    a=ap.parse_args()
    v29=json.loads(Path(a.v29).read_text(encoding='utf-8'))
    records=v29['records']
    if len(records)!=48:
        raise SystemExit(f'expected 48 HemEnv/HemEnvLerp records, got {len(records)}')
    labels=[r['label'] for r in records]
    hashes=[r['original_sha256'] for r in records]
    if len(set(labels))!=48 or len(set(hashes))!=48:
        raise SystemExit('UL48 labels/hashes must be unique')
    lines=[
      '#pragma once\n#include <array>\n#include <cstdint>\n#include <string_view>\n\n',
      'namespace dsrrl::runtime::mr {\n',
      'struct ul_plan { std::uint8_t index; std::string_view label, original_sha256; bool lerp; std::int8_t mr_stable_index; };\n',
      'inline constexpr std::array<ul_plan,48> k_ul_plans = {{\n'
    ]
    for i,r in enumerate(records):
        stable=-1 if r['lerp'] else int(r['stable_index'])
        lines.append(
            '  {'+','.join([
                f'{i}u',q(r['label']),q(r['original_sha256']),
                'true' if r['lerp'] else 'false',str(stable)
            ])+'},\n'
        )
    lines += [
      '}};\n',
      '} // namespace dsrrl::runtime::mr\n'
    ]
    Path(a.out).write_text(''.join(lines),encoding='utf-8')
    return 0

if __name__=='__main__':
    raise SystemExit(main())
