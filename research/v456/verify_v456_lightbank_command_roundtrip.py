#!/usr/bin/env python3
"""V456 original DSR SHA-locked per-model LightBank command + timed DrawParam decoder RE.

Read-only static EXE audit. No runtime hooks or row edits.
WITNESSES file is part of the source and pins exact instruction bytes at each RVA.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import struct
from pathlib import Path

BASE = 0x140000000
EXE_SHA = 'a45aaa36dd2f6cc151670a639ea5547043cf38ea79ff4178b963c6ed71f98d7b'
WITNESS_FILE = Path(__file__).with_name('V456_OPCODE_WITNESSES.json')

# CPU call edges at exact native instruction sites.
EDGES = {
    0x20B546:0x21C970,          # ChrModel vtable update -> inherited FrpgModel update
    0x21CA6C:0x21F600,          # packed selector #0 (+0x93)
    0x21CA7C:0x21F5C0,          # packed selector #1 (+0x94)
    0x21CA89:0x21F640,          # packed selector #2 (+0x95)
    0x21CAC7:0x222AC0,         # command encoder 0x804c
    0x22346C:0x222B60,         # command receiver selected by jump table
    0x222BBB:0x1D7580,         # DrawEntity+0x108 -> DrawParam state
    0x1D75AA:0x1D7820,         # lane 1 interpolation
    0x1D75C0:0x1D7820,         # lane 2 interpolation
    0x1D75DB:0x1D7820,         # lane 3 interpolation
    0x357BFD:0x21F600,         # PlayerIns virtual -> own model select
    0x357C37:0x21F5C0,         # PlayerIns second selector
    0x21C554:0x157CF0,         # original FrpgModel registration
    0x21C71F:0x157CF0,         # alternate FrpgModel registration
}
# These are MSVC vtable slots in .rdata (pointer values include image base).
VTABLE_SLOTS = {
    0x13251F0+0xC0:0x357AE0, # PlayerIns method which reads own ChrModel+0x93
    0x12EAF30+0x20:0x20B4E0, # ChrModel update
    0x12EC838+0x20:0x21C970, # base FrpgModel update
}

class PE:
    def __init__(self, b:bytes|bytearray):
        self.raw=b
        assert b[:2]==b'MZ', 'Not an MZ image'
        pe=struct.unpack_from('<I',b,0x3C)[0]
        assert b[pe:pe+4]==b'PE\0\0', 'Not PE'
        assert struct.unpack_from('<H',b,pe+4)[0]==0x8664, 'Not AMD64'
        n=struct.unpack_from('<H',b,pe+6)[0]
        osz=struct.unpack_from('<H',b,pe+20)[0]
        self.sections=[]
        for i in range(n):
            o=pe+24+osz+40*i
            _,rva,sz,ptr=struct.unpack_from('<IIII',b,o+8)
            name=b[o:o+8].split(b'\0')[0].decode('ascii','replace')
            self.sections.append((rva,sz,ptr,name))
    def pos(self,rva,n):
        for a,sz,o,name in self.sections:
            if a<=rva and rva+n<=a+sz:return o+rva-a
        raise AssertionError(f'RVA not mapped {rva:#x}')
    def data(self,rva,n):
        p=self.pos(rva,n)
        return self.raw[p:p+n]
    def call(self,site):
        b=self.data(site,5)
        assert b[0]==0xE8,f'Expected E8 at {site:#x}'
        return site+5+struct.unpack_from('<i',b,1)[0]
    def u64(self,site):
        return struct.unpack('<Q',self.data(site,8))[0]
    def i32(self,site):
        return struct.unpack('<i',self.data(site,4))[0]


def pack_selector(model92:int, slot:int)->int:
    """Forward of native packers 0x21F600/5C0/640 for signed byte inputs.

    This tests integer encoding only; it does not model illumination or pixels.
    """
    assert -128<=model92<=127 and -128<=slot<=127
    group=model92 >> 3
    if group < 0 or slot < 0: return 0xFFFFFFFF
    return ((group&0x7F)<<8)|(slot&0xFF)


def verify(raw,check_sha=True):
    if check_sha:assert hashlib.sha256(raw).hexdigest()==EXE_SHA, 'Wrong original EXE SHA256'
    p=PE(raw)
    witnesses=json.loads(WITNESS_FILE.read_text(encoding='utf-8'))
    for w in witnesses:
        addr=int(w['rva'],16)
        expected=bytes.fromhex(w['bytes'])
        assert p.data(addr,len(expected))==expected, f'Opcode preimage at {addr:#x}: {w["semantics"]}'
    for site,dest in EDGES.items():
        assert p.call(site)==dest,(hex(site),hex(dest))
    for vt,dest in VTABLE_SLOTS.items():
        assert p.u64(vt)==BASE+dest,(hex(vt),hex(dest))
    # Writer opcode 0x804c, decoder range is 0x8046 .. 0x8058
    assert p.data(0x222AD2,5)==bytes.fromhex('ba4c800000')
    assert p.data(0x223386,6)==bytes.fromhex('8d82ba7fffff') # lea eax,[rdx-0x8046]
    assert p.i32(0x2235c8 + 6*4)==0x223466, 'Opcode 0x804C jump destination'
    # The 0x804c case invokes 0x222B60, which applies to draw entity+0x108.
    assert p.call(0x22346C)==0x222B60
    assert p.call(0x222BBB)==0x1D7580
    # Functional forward cross-checks: no inference of authored PTDE row.
    assert pack_selector(3*8+7,30)==0x31E
    assert pack_selector(3*8+7,31)==0x31F
    assert pack_selector(-1,30)==0xFFFFFFFF
    assert pack_selector(0,-1)==0xFFFFFFFF
    return {
       'status':'STATIC_PASS','exe_sha256':EXE_SHA,
       'instruction_witnesses':len(witnesses),'direct_call_edges':len(EDGES),
       'vtable_slots':len(VTABLE_SLOTS),'opcode': '0x804C',
       'dispatch_jump_table_rva':'0x2235E0','dispatch_case_rva':'0x223466',
       'reader':'0x222B60','receiver':'DrawEntity+0x108 FrpgDrawParam',
       'three_16bit_selector_lanes':['+0x8','+0x18','+0x28'],
       'temporal_index_transition_helper':'0x1D7820',
       'per_player_secondary_copy':'PlayerIns+0x60 model+0x93 -> PlayerIns+0x850 controller+0xAB',
       'per_model_registration_handle':'FrpgModel+0x10, NOT confirmed equal to Seed+0x10',
       'actor_player_enemy_to_RX34':'OPEN',
       'row30_31_same_actor_lerp':'HYPOTHESIS_NOT_RUNTIME_PROVEN',
       'nearby_NPC_pollution':'OPEN','pixel_equivalence':'OPEN','addon_changed':False,
    }

def mutation_test(raw):
    p=PE(raw)
    witnesses=json.loads(WITNESS_FILE.read_text(encoding='utf-8'))
    sites={int(x['rva'],16) for x in witnesses}
    sites.update(VTABLE_SLOTS)
    sites.add(0x2235e0)
    arr=bytearray(raw)
    failures=[]
    for addr in sorted(sites):
        loc=p.pos(addr,1)
        arr[loc]^=1
        try:verify(arr,check_sha=False)
        except (AssertionError,ValueError,IndexError,struct.error):pass
        else:failures.append(f'{addr:#x}')
        arr[loc]^=1
    arr[-1]^=1
    try:verify(arr,check_sha=True)
    except AssertionError:pass
    else:failures.append('SHA_FAILURE_UNDETECTED')
    return {'attempted':len(sites)+1,'failed':failures,'status':'PASS' if not failures else 'FAIL'}

if __name__=='__main__':
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('original_exe',type=Path)
    ap.add_argument('--mutations',action='store_true')
    ap.add_argument('--output',type=Path)
    args=ap.parse_args()
    raw=args.original_exe.read_bytes()
    result=verify(raw)
    if args.mutations:result['negative_mutations']=mutation_test(raw)
    text=json.dumps(result,indent=2,ensure_ascii=False)+'\n'
    print(text)
    if args.output:args.output.write_text(text,encoding='utf-8')
