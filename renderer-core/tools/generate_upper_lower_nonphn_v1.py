#!/usr/bin/env python3
from __future__ import annotations

import argparse
import csv
from pathlib import Path


def q(value: str) -> str:
    return '"' + value.replace('\\', '\\\\').replace('"', '\\"') + '"'


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument('--input', action='append', required=True)
    ap.add_argument('--output', required=True)
    args = ap.parse_args()

    rows = []
    for source in args.input:
        with Path(source).open('r', encoding='utf-8', newline='') as fh:
            data = [line for line in fh if not line.startswith('#')]
        for raw in csv.DictReader(data, delimiter='\t'):
            rows.append({
                'plan_index': int(raw['plan_index']),
                'shader_index': int(raw['shader_index']),
                'stable_receiver_id': int(raw['stable_receiver_id']),
                'stratum': raw['stratum'],
                'family': raw['family'],
                'declaration_word': int(raw['declaration_word']),
                'name': raw['name'],
                'stock_size': int(raw['stock_size']),
                'stock_sha256': raw['stock_sha256'],
                'u_slot_word': int(raw['u_slot_word']),
                'd_slot_word_0': int(raw['d_slot_word_0']),
                'd_slot_word_1': int(raw['d_slot_word_1']),
                'replacement_size': int(raw['replacement_size']),
                'replacement_sha256': raw['replacement_sha256'],
            })

    rows.sort(key=lambda r: r['plan_index'])

    if len(rows) != 164:
        raise SystemExit(f'expected 164 non-Phn U/L executable consumers, got {len(rows)}')
    if [r['plan_index'] for r in rows] != list(range(180, 344)):
        raise SystemExit('plan_index must be contiguous 180..343')
    if len({r['stock_sha256'] for r in rows}) != len(rows):
        raise SystemExit('duplicate stock SHA')
    if len({r['replacement_sha256'] for r in rows}) != len(rows):
        raise SystemExit('duplicate replacement SHA')

    expected = {
        'gst': 72,
        'gst_faceeye': 6,
        'sfx': 72,
        'snow': 11,
        'ntoa': 3,
    }
    for family, count in expected.items():
        if sum(r['family'] == family for r in rows) != count:
            raise SystemExit(f'{family} count mismatch')

    if sum(r['declaration_word'] == 7 for r in rows) != 6:
        raise SystemExit('expected 6 FaceEye declaration-word=7 plans')
    if sum(r['declaration_word'] == 11 for r in rows) != 158:
        raise SystemExit('expected 158 ordinary declaration-word=11 plans')

    for r in rows:
        if r['family'] not in expected:
            raise SystemExit('invalid family')
        if r['stratum'] not in ('spc', 'nospc'):
            raise SystemExit('invalid stratum')
        if r['declaration_word'] not in (7, 11):
            raise SystemExit('invalid declaration boundary')
        if not (r['u_slot_word'] < r['d_slot_word_0'] < r['d_slot_word_1']):
            raise SystemExit(f'invalid patch ordering for {r["name"]}')
        if len(r['stock_sha256']) != 64 or len(r['replacement_sha256']) != 64:
            raise SystemExit('invalid SHA length')

    out = [
        '#pragma once\n',
        '#include <array>\n',
        '#include <cstdint>\n',
        '#include <string_view>\n\n',
        'namespace dsrrl::operators::lightbank::generated_nonphn {\n\n',
        'enum class upper_lower_nonphn_stratum : std::uint8_t { nospc = 0, spc };\n',
        'enum class upper_lower_nonphn_family : std::uint8_t { gst = 0, gst_faceeye, sfx, snow, ntoa };\n\n',
        'struct upper_lower_nonphn_plan {\n',
        '    std::uint16_t plan_index;\n',
        '    std::uint16_t shader_index;\n',
        '    std::uint8_t stable_receiver_id;\n',
        '    upper_lower_nonphn_stratum stratum;\n',
        '    upper_lower_nonphn_family family;\n',
        '    std::uint8_t declaration_word;\n',
        '    std::string_view name;\n',
        '    std::uint32_t stock_size;\n',
        '    std::string_view stock_sha256;\n',
        '    std::uint32_t u_slot_word;\n',
        '    std::uint32_t d_slot_word_0;\n',
        '    std::uint32_t d_slot_word_1;\n',
        '    std::uint32_t replacement_size;\n',
        '    std::string_view replacement_sha256;\n',
        '};\n\n',
        'inline constexpr std::array<upper_lower_nonphn_plan,164> k_upper_lower_nonphn_plans = {{\n',
    ]

    for r in rows:
        stratum = 'upper_lower_nonphn_stratum::' + r['stratum']
        family = 'upper_lower_nonphn_family::' + r['family']
        out.append(
            '    {' +
            f"{r['plan_index']}u,{r['shader_index']}u,{r['stable_receiver_id']}u,{stratum},{family}," +
            f"{r['declaration_word']}u,{q(r['name'])},{r['stock_size']}u,{q(r['stock_sha256'])}," +
            f"{r['u_slot_word']}u,{r['d_slot_word_0']}u,{r['d_slot_word_1']}u," +
            f"{r['replacement_size']}u,{q(r['replacement_sha256'])}" +
            '},\n'
        )

    out += [
        '}};\n\n',
        '} // namespace dsrrl::operators::lightbank::generated_nonphn\n',
    ]

    Path(args.output).write_text(''.join(out), encoding='utf-8')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
