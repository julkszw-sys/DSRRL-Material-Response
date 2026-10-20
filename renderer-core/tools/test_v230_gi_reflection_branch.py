from pathlib import Path
root=Path(__file__).resolve().parents[1]
src=(root/'src/runtime/texture_identity_transport.cpp').read_text()
asm=(root/'integrated/texture_identity_detour.asm').read_text()
build=(root/'tools/build_v230_production.ps1').read_text()
wf=(root.parent/'.github/workflows/v230-production-from-v359.yml').read_text()
for needle in ('0x2017F4u','0x12E9D40u','0x0CF290u','0x44,0x89,0x7C,0x24,0x20,0x48,0x8D,0x15','GI REFLECTION SOURCE HOOK','restore(g_gi_reflection)','source_to_native_join=NOT_ESTABLISHED','action=OBSERVE_ONLY_NO_ALIAS'):
    assert needle in src, needle
for needle in ('dsrrl_gi_reflection_branch_entry PROC','mov ecx, r9d','mov edx, r15d','call dsrrl_gi_reflection_branch_observer','mov dword ptr [rsp+20h], r15d','call qword ptr [g_dsrrl_gi_reflection_format_call]'):
    assert needle in asm,needle
assert 'DSRRL_V230_GI_REFLECTION_SOURCE_BRANCH' in build
assert 'test_v230_gi_reflection_branch.py' in wf
print('V230_GI_REFLECTION_EXACT_SOURCE_BRANCH_CONTRACT_PASS no_alias=1')
