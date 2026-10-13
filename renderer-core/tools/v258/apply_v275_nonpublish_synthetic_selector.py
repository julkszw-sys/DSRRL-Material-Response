#!/usr/bin/env python3
"""V275: suppress EXACT known synthetic selector source publication.

Only ret=0x20EB7F and A=B=0,beta=0. Full-builder legitimate bank0
is untouched. The legacy source hook is an independent fallback: no
claim of entire draw fail-open or visible PTDE improvement.
"""
import argparse
from pathlib import Path
P = Path('renderer-core/src/runtime/pmetal_env_source_runtime.cpp')
ANCHOR = '''    if (descriptor_owner != owner ||
        character > 1u)
        return;'''
MARKER = '[DSRRL V275 SYNTHETIC ZERO SELECTOR NONPUBLISH]'
REPLACEMENT = ANCHOR + r'''

    // V275: alternate DSR builder at return 20EB7F synthesizes a zero
    // selector tuple WITHOUT the authored LightBank producer. A real
    // 0/0/0 tuple from full builder 20E019 is NOT excluded.
    if (pmetal_selector_policy::is_synthetic_zero_descriptor(
            rva, raw_a, raw_b, beta)) {
        static std::atomic_bool v275_synthetic_logged{false};
        if (!v275_synthetic_logged.exchange(
                true, std::memory_order_relaxed))
            reshade::log::message(
                reshade::log::level::info,
                "[DSRRL V275 SYNTHETIC ZERO SELECTOR NONPUBLISH] "
                "site=20EB7F a=0000 b=0000 beta=0 "
                "source_publish=SUPPRESSED "
                "legacy_fallback=SEPARATE_UNVERIFIED "
                "draw_failopen=NOT_PROVEN PTDE_pixel=OPEN");
        return;
    }'''
def run(apply):
    s=P.read_text(encoding='utf8').replace('\r\n','\n')
    if MARKER in s:
        if (s.count(MARKER)!=1 or s.count('v275_synthetic_logged')!=2 or
            s.count('pmetal_selector_policy::is_synthetic_zero_descriptor(')!=1):
            raise RuntimeError('V275 duplicated or incomplete source')
        print('V275_SYNTHETIC_ZERO_SELECTOR_NONPUBLISH_SOURCE_ORACLE_PASS');return
    if s.count(ANCHOR)!=1 or 'pmetal_selector_policy.hpp' not in s:
        raise RuntimeError('V275 exact original selector preimage mismatch')
    if not apply: raise RuntimeError('V275 overlay missing')
    P.write_text(s.replace(ANCHOR,REPLACEMENT,1),encoding='utf8',newline='\n')
    print('V275_SYNTHETIC_ZERO_SELECTOR_NONPUBLISH_SOURCE_APPLIED')
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--apply',action='store_true')
    a=p.parse_args();run(a.apply)
