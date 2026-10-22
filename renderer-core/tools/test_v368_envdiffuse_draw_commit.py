from pathlib import Path
root=Path(__file__).resolve().parents[1]
s=(root/'integrated/integrated_addon.cpp').read_text(encoding='utf-8')
c=(root/'integrated/CMakeLists.txt').read_text(encoding='utf-8')
assert 'DSRRL_V368_ENVDIFFUSE_DRAW_COMMIT_PROOF' in c
assert s.count('v368_envdiffuse_dispatch_witness(cmd_list,prepared.batch.mutation,') == 2
assert s.count('[DSRRL V368 ENVDIFFUSE DRAW COMMIT]') == 1
assert 'if(mutation.owners!=bit) return;' in s
assert 'draw_tx_issued(result)' in s and 'issued_restored' in s
assert 'context->GetType()' in s
assert 'sample>=16u' in s
print('V368 exact EnvDiffuse post-dispatch source witness PASS; RUNTIME STILL OPEN')
