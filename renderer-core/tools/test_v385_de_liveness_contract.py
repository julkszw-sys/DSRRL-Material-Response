from pathlib import Path
s=Path('renderer-core/src/runtime/flver_engine_hooks.cpp').read_text()
assert s.index('g_v385_de_hit_total.fetch_add')<s.index('if(site!=k_v384_type180_a_return&&site!=k_v384_type180_b_return)')
assert '[DSRRL V385 DE HEALTH]' in s
assert '[DSRRL V385 DE OTHER CALLER]' in s
assert 'g_v384_de_original(model,dest)' in s
print('V385 source contract PASS')
