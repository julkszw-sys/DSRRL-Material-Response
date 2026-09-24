#!/usr/bin/env python3
"""Generate the Core 151 V13 PTDE P_Metal donor table from immutable 1.45 provenance."""
from pathlib import Path
import re

ROOT=Path(__file__).resolve().parents[2]
SRC=ROOT/'renderer-core/provenance/mr145/SOURCE_V13_PTDE_DONOR.cpp'
OUT=ROOT/'runtime-v1/generated/v13_pmetal_donors.hpp'

s=SRC.read_text(encoding='utf-8')
rows=[tuple(map(int,m.groups())) for m in re.finditer(r'\{(\d+)u,(\d+)u,(\d+)u,(\d+)u,(\d+)u\}',s)]
banks=[(m.group(1),int(m.group(2)),int(m.group(3))) for m in re.finditer(r'\{(0x[0-9a-fA-F]+)ULL,(\d+)u,(\d+)u\}',s)]
if len(rows)!=1246 or len(banks)!=20:
    raise SystemExit(f'unexpected V13 table counts rows={len(rows)} banks={len(banks)}')

lines=[
'#pragma once',
'// Generated from immutable Material Response 1.45 recovery source:',
'// recovered-source/1.45/pre_v12/mr10_v14/SOURCE_V13_PTDE_DONOR.cpp',
'// Do not hand-edit. Core 151 reuses the V13 PTDE P_Metal donor data only;',
'// historical hook/trampoline ownership is intentionally not imported.',
'',
'#include <array>',
'#include <cstddef>',
'#include <cstdint>',
'',
'namespace dsrrl::runtime::pmetal145 {',
'',
'struct donor_row { std::uint32_t id; std::uint16_t r, g, b, m; };',
'struct bank_donor { std::uint64_t signature; std::uint32_t first; std::uint32_t count; };',
'',
f'inline constexpr std::array<donor_row, {len(rows)}> k_rows = {{{{',
]
lines += [f'    donor_row{{{a}u,{b}u,{c}u,{d}u,{e}u}},' for a,b,c,d,e in rows]
lines += ['}};',f'inline constexpr std::array<bank_donor, {len(banks)}> k_banks = {{{{']
lines += [f'    bank_donor{{{sig}ULL,{first}u,{count}u}},' for sig,first,count in banks]
lines += [
'}};',
'',
'constexpr const bank_donor *find_bank(std::uint64_t signature) noexcept',
'{',
'    for (const auto &b : k_banks) if (b.signature == signature) return &b;',
'    return nullptr;',
'}',
'',
'constexpr const donor_row *find_row(const bank_donor &bank,std::uint32_t id) noexcept',
'{',
'    if (bank.first > k_rows.size() || bank.count > k_rows.size() - bank.first) return nullptr;',
'    for (std::uint32_t i=0;i<bank.count;++i) {',
'        const auto &r=k_rows[bank.first+i];',
'        if (r.id==id) return &r;',
'    }',
'    return nullptr;',
'}',
'',
'} // namespace dsrrl::runtime::pmetal145',
''
]
OUT.parent.mkdir(parents=True,exist_ok=True)
OUT.write_text('\n'.join(lines),encoding='utf-8')
print(f'generated {OUT} rows={len(rows)} banks={len(banks)}')
