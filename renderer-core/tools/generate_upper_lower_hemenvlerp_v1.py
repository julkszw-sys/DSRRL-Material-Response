#!/usr/bin/env python3
from __future__ import annotations

import argparse
import csv
from pathlib import Path


def q(value: str) -> str:
    return '"' + value.replace('\\', '\\\\').replace('"', '\\"') + '"'


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument('--input', required=True)
    ap.add_argument('--output', required=True)
    args = ap.parse_args()

    rows = []
    with Path(args.input).open('r', encoding='utf-8', newline='') as fh:
        data = [line for line in fh if not line.startswith('#')]
    reader = csv.DictReader(data, delimiter='\t')

    for raw in reader:
        row = {
            'plan_index': int(raw['plan_index']),
            'shader_index': int(raw['shader_index']),
            'stable_receiver_id': int(raw['stable_receiver_id']),
            'stratum': raw['stratum'],
            'name': raw['name'],
            'stock_size': int(raw['stock_size']),
            'stock_sha256': raw['stock_sha256'],
            'u_slot_word': int(raw['u_slot_word']),
            'd_slot_word_0': int(raw['d_slot_word_0']),
            'd_slot_word_1': int(raw['d_slot_word_1']),
            'replacement_size': int(raw['replacement_size']),
            'replacement_sha256': raw['replacement_sha256'],
        }
        rows.append(row)

    rows.sort(key=lambda r: r['plan_index'])

    if len(rows) != 36:
        raise SystemExit(f'expected 36 Upper/Lower HemEnv consumers, got {len(rows)}')
    if [r['plan_index'] for r in rows] != list(range(36, 72)):
        raise SystemExit('plan_index must be contiguous 36..71')

    spc = [r for r in rows if r['stratum'] == 'spc']
    nospc = [r for r in rows if r['stratum'] == 'nospc']
    if len(spc) != 24 or len(nospc) != 12:
        raise SystemExit('expected 24 Spc + 12 no-Spc consumers')

    if sorted(r['stable_receiver_id'] for r in spc) != list(range(24, 48)):
        raise SystemExit('Spc stable receiver IDs must be exactly 24..47')
    if any(r['stable_receiver_id'] != 0 for r in nospc):
        raise SystemExit('no-Spc consumers must not borrow stable HemEnv receiver IDs')

    if len({r['shader_index'] for r in rows}) != len(rows):
        raise SystemExit('duplicate shader_index')
    if len({r['stock_sha256'] for r in rows}) != len(rows):
        raise SystemExit('duplicate stock SHA')
    if len({r['replacement_sha256'] for r in rows}) != len(rows):
        raise SystemExit('duplicate replacement SHA')

    for r in rows:
        if r['stratum'] not in ('spc', 'nospc'):
            raise SystemExit('invalid stratum')
        if not (r['u_slot_word'] < r['d_slot_word_0'] < r['d_slot_word_1']):
            raise SystemExit(f'invalid patch ordering for {r["name"]}')
        if len(r['stock_sha256']) != 64 or len(r['replacement_sha256']) != 64:
            raise SystemExit('invalid SHA length')

    out = [
        '#pragma once\n',
        '#include <array>\n',
        '#include <cstdint>\n',
        '#include <string_view>\n\n',
        'namespace dsrrl::operators::lightbank::generated_lerp {\n\n',
        'enum class upper_lower_hemenvlerp_stratum : std::uint8_t { nospc = 0, spc };\n\n',
        'struct upper_lower_hemenvlerp_plan {\n',
        '    std::uint16_t plan_index;\n',
        '    std::uint16_t shader_index;\n',
        '    std::uint8_t stable_receiver_id;\n',
        '    upper_lower_hemenvlerp_stratum stratum;\n',
        '    std::string_view name;\n',
        '    std::uint32_t stock_size;\n',
        '    std::string_view stock_sha256;\n',
        '    std::uint32_t u_slot_word;\n',
        '    std::uint32_t d_slot_word_0;\n',
        '    std::uint32_t d_slot_word_1;\n',
        '    std::uint32_t replacement_size;\n',
        '    std::string_view replacement_sha256;\n',
        '};\n\n',
        'inline constexpr std::array<upper_lower_hemenvlerp_plan,36> k_upper_lower_hemenvlerp_plans = {{\n',
    ]

    for r in rows:
        stratum = (
            'upper_lower_hemenvlerp_stratum::spc'
            if r['stratum'] == 'spc'
            else 'upper_lower_hemenvlerp_stratum::nospc'
        )
        out.append(
            '    {' +
            f"{r['plan_index']}u,{r['shader_index']}u,{r['stable_receiver_id']}u,{stratum}," +
            f"{q(r['name'])},{r['stock_size']}u,{q(r['stock_sha256'])}," +
            f"{r['u_slot_word']}u,{r['d_slot_word_0']}u,{r['d_slot_word_1']}u," +
            f"{r['replacement_size']}u,{q(r['replacement_sha256'])}" +
            '},\n'
        )

    out += [
        '}};\n\n',
        '} // namespace dsrrl::operators::lightbank::generated_lerp\n',
    ]

    Path(args.output).write_text(''.join(out), encoding='utf-8')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
