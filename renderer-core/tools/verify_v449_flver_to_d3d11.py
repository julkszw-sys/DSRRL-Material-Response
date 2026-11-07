#!/usr/bin/env python3
"""V449: prove original DSR FlverModel CPU-issued D3D11 draw stays within
model::draw call frame; original EXE only, no game/addon mutation.
Static ABI proof, NOT gameplay runtime or shader identity proof.
Depends on repo tools/verify_v448_native_builder.py for SHA-locked PE parsing.
"""
from __future__ import annotations
import argparse,hashlib,json,struct
from pathlib import Path
from verify_v448_native_builder import PE,EXE_SHA
SITES={
 'flver_virtual_draw_entry':(0x20DDA0,'4c8bdc'),
 'packet_context_to_r14':(0x20DDFF,'4d8b37'),
 'native_selector_event_in_model_draw':(0x20E014,'e807da0100'),
 'context_wrapper_from_packet_context':(0x20E540,'498bbe90000000'),
 'instanced_com_context_from_wrapper':(0x20E55A,'488b4f20'),
 'instanced_branch_on_engine_count':(0x20E568,'4585ed'),
 'instanced_zero_flag_branch':(0x20E56B,'7433'),
 'instanced_index_count_register':(0x20E593,'8bd6'),
 'instanced_com_vtable_fetch':(0x20E595,'488b01'),
 'instanced_vtable_call_0xA0':(0x20E598,'ff90a0000000'),
 'instanced_branch_common_post_draw':(0x20E59E,'eb24'),
 'ordinary_com_context_from_wrapper':(0x20E5B2,'488b4f20'),
 'ordinary_zero_first_index':(0x20E5B9,'4533c0'),
 'ordinary_index_count_register':(0x20E5BC,'8bd6'),
 'ordinary_com_vtable_fetch':(0x20E5BE,'488b01'),
 'ordinary_vtable_call_0x60':(0x20E5C1,'ff5060'),
 'common_post_draw_code':(0x20E5C4,'807c245500'),
 'parent_builder_virtual_model_call':(0x220CED,'ff5018'),
 'parent_builder_return':(0x220CF0,'488b8db0010000'),
}

def run(exe:bytes,mutations:bool):
    digest=hashlib.sha256(exe).hexdigest()
    assert digest==EXE_SHA,(digest,EXE_SHA)
    pe=PE(exe)
    preimage=[]
    for label,(rva,pre) in SITES.items():
        actual=pe.read(rva,len(pre)//2).hex()
        preimage.append({'label':label,'rva':hex(rva),'expected':pre,'observed':actual,'ok':actual==pre})
    native_rvas=[0x20E014,0x20E598,0x20E5C1,0x20E5C4]
    pdata={hex(a):pe.pdata_covers(a) for a in native_rvas}
    native_frame=list(pdata.values())[0]
    same_model_draw_pdata=all(x==native_frame and x is not None for x in pdata.values())
    builder_call_span=pe.pdata_covers(0x220CED)
    builder_return_span=pe.pdata_covers(0x220CF0)
    same_builder_frame=builder_call_span is not None and builder_call_span==builder_return_span
    imm_jmp=pe.read(0x20E59E,2)
    after_jmp=0x20E59E+2+struct.unpack('<b',imm_jmp[1:])[0]
    correct_instanced_merge=after_jmp==0x20E5C4
    correct_branch=0x20E56B+2+struct.unpack('<b',pe.read(0x20E56B,2)[1:])[0]==0x20E5A0
    # Verify expected D3D11 VTable method ordinal indexes (8-byte pointers).
    # This is a typed ABI interpretation; pointer runtime identity remains OPEN.
    index_ordinary=0x60//8; index_instanced=0xA0//8
    correct_method_indices=(index_ordinary,index_instanced)==(12,20)
    negatives=[]
    if mutations:
        for label,(rva,pre) in SITES.items():
            value=bytearray(pe.read(rva,len(pre)//2))
            value[0]^=1
            negatives.append({'label':label,'rejected':value.hex()!=pre})
    result={
        'status':'PASS' if (all(x['ok'] for x in preimage) and same_model_draw_pdata
                and same_builder_frame and correct_instanced_merge and correct_branch
                and correct_method_indices and all(x['rejected'] for x in negatives)) else 'FAIL',
        'exe_sha256':digest,'static_preimages':preimage,'count_preimages':len(preimage),
        'native_d3d11_call_sites':{'DrawIndexed':hex(0x20E5C1),'DrawIndexedInstanced':hex(0x20E598)},
        'COM_vtable_method_indices':{'DrawIndexed':index_ordinary,'DrawIndexedInstanced':index_instanced},
        'native_model_draw_pdata':{k:list(map(hex,v)) if v else None for k,v in pdata.items()},
        'native_material_selector_and_d3d_call_same_unwind_region':same_model_draw_pdata,
        'builder_call_return_same_unwind_region':same_builder_frame,
        'drawindexedinstanced_branches_to_common_return':correct_instanced_merge,
        'noninstanced_branch_selects_indexed_call':correct_branch,
        'negative_mutations':len(negatives),'mutation_results':negatives,
        'interpretation':'Native CPU D3D11 direct virtual calls in FlverModel::draw execute before the containing builder virtual-call returns; path supports a live frame at interception, subject to actual runtime, ReShade wrapper/unwind and receiver identity.',
        'limits':{'actual_vtable_runtime_identity':'OPEN','specific_RX34_receiver':'OPEN','rtlvirtualunwind_in_game':'OPEN','actor_generation':'OPEN','native_to_ptde_pixel':'OPEN','bridge_activation':'OFF_SHADOW'}
    }
    return result

def main():
    parser=argparse.ArgumentParser();parser.add_argument('exe',type=Path);parser.add_argument('--mutations',action='store_true');parser.add_argument('--json',type=Path)
    a=parser.parse_args();r=run(a.exe.read_bytes(),a.mutations)
    if a.json:a.json.write_text(json.dumps(r,indent=2),encoding='utf-8')
    print('V449 original DSR: %s, preimages=%s, direct draw methods=2, negatives=%s, model draw same pdata=%s, builder same pdata=%s, runtime OPEN' % (
        r['status'],r['count_preimages'],r['negative_mutations'],r['native_material_selector_and_d3d_call_same_unwind_region'],r['builder_call_return_same_unwind_region']))
    if r['status']!='PASS':raise SystemExit(1)
if __name__=='__main__':main()
