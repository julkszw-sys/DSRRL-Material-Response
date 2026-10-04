"""Generate exact original-bank donors for the draw-scoped PTDE operator."""
import argparse
import struct
from pathlib import Path
from audit_pointlight_ptde_source_corpus import banks

p = argparse.ArgumentParser()
p.add_argument('--ptde', required=True)
p.add_argument('--dsr', required=True)
p.add_argument('--output', required=True)
a = p.parse_args()
target, host = banks(a.ptde, True), banks(a.dsr, False)
seen = {}
lines = ['#pragma once', '#include <array>', '#include <cstdint>',
         'namespace dsrrl::runtime::pointlight_donors {',
         'struct row { std::uint32_t begin_bits, end_bits; std::int16_t r,g,b,intensity; };',
         'static_assert(sizeof(row)==16);',
         'struct bank { std::array<row,64> dsr, ptde; };',
         f'inline constexpr std::array<bank,{len(target)}> banks = {{{{']
for name in sorted(target):
    d, t = host[name]['rows'], target[name]['rows']
    if len(d) != 64 or len(t) != 64:
        raise ValueError('Unexpected bank size')
    key = tuple(tuple(r) for r in d)
    if key in seen and seen[key] != t:
        raise ValueError('Ambiguous DSR bank identity')
    seen[key] = t
    lines += [f'// {name}', 'bank{{{']
    for rows in (d,t):
        if rows is t: lines += ['}},{{']
        for r in rows:
            begin, end = struct.unpack('<II',struct.pack('<ff',*r[:2]))
            lines += [f'row{{0x{begin:08x}u,0x{end:08x}u,{r[2]},{r[3]},{r[4]},{r[5]}}},']
    lines += ['}}},']
lines += ['}};', '} // namespace dsrrl::runtime::pointlight_donors']
Path(a.output).write_text('\n'.join(lines)+'\n',encoding='utf-8')
