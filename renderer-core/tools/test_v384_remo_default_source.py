#!/usr/bin/env python3
"""V384 static, exact-retail read-only Type180 +DE function hook contract."""
from pathlib import Path
import re
root=Path(__file__).resolve().parents[1]
s=(root/'src/runtime/flver_engine_hooks.cpp').read_text()
build=(root/'tools/build_v230_production.ps1').read_text()
# Verified separately against SHA-pinned retail exe using llvm-objdump.
original='0f b7 81 de 00 00 00 66 89 02 48 8b c2 c3'
m=re.search(r'k_v384_de_bytes\s*=\s*\{([^}]+)\}',s)
assert m is not None
actual=bytes(int(x,16) for x in re.findall(r'0x([0-9a-fA-F]{2})\b',m.group(1)))
assert actual.hex(' ')==original,(actual.hex(' '),original)
assert s.count('v384_de_observer(')==1
assert 'g_v384_de_original(model,dest)' in s
assert 'k_v384_de_helper=0x572890u' in s
assert 'k_v384_type180_a_return=0x56f842u' in s
assert 'k_v384_type180_b_return=0x56f875u' in s
assert s.count('prep(g_v384_de,k_v384_de_helper,k_v384_de_bytes')==1
assert 'restore(g_v384_de)' in s
assert 'SIGNATURE_MISMATCH_FAILOPEN' in s
assert 'status=HOOK_ACTIVE exact_exe=1' in s
assert '[DSRRL V384 REMO TYPE180 DE]' in s
assert s.index('g_v384_de_original(model,dest)')<s.index('std::memcpy(&def')
assert 'V384 REMO TYPE180 DE' in build
assert 'V384 REMO DE INSTALL' in build
assert 'source.row_id_a = 5' not in s
print('V384 exact-retail negative Type180 +DE read-only hook contract PASS')
