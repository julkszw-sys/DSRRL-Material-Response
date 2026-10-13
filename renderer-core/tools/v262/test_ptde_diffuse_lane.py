#!/usr/bin/env python3
"""Synthetic DXBC regression for V262 code mutation; no original DSR assets."""
import hashlib,struct
import ptde_diffuse_first_lane_dxbc_candidate as op

def u32(words):
    return struct.pack("<"+"I"*len(words),*words)
def fake_dxbc(with_cb12):
    e=[0x0400001f,0x0020800a,0,85]
    mul=[0x08000038,0x00100072,1,0x00100246,1,0x00208246,0,9]
    log=[0x0600002f,0x00100072,1,0x80100246,0x81,1]
    pow=[0x0a000038,0x00100072,1,0x00100246,1,0x00004002,
         0x400ccccd,0x400ccccd,0x400ccccd,0]
    exp=[0x05000019,0x00100072,9,0x00100246,1]
    mul_sat=[0x08002038,0x00100072,1,0x00100796,7,0x00208246,0,10]
    log2=[0x0500002f,0x00100072,1,0x00100246,1]
    exp2=[0x05000019,0x00100072,11,0x00100246,1]
    head=[0x04000059,0x00208e46,12,6] if with_cb12 else []
    fog=[0x0100003a]*212
    op.FOG_SHA=hashlib.sha256(u32(fog)).hexdigest()
    words=[0x00000050,0]+head+e+[0x01000012]+mul+log+pow+exp+mul_sat+log2+pow+exp2+[0x01000015]+fog
    words[1]=len(words)
    chunk=b"SHEX"+struct.pack("<I",len(words)*4)+u32(words)
    header=bytearray(b"DXBC"+b"\0"*16+struct.pack("<III",1,36+len(chunk),1)+struct.pack("<I",36))
    assert len(header)==36
    data=header+chunk
    data[4:20]=op.dxbc_digest(data)
    return bytes(data)

def run():
    # The synthetic fixture uses receiver 33's exact *operand topology*.
    for has_b12 in (False,True):
        src=fake_dxbc(has_b12)
        patched,info=op.patch(src,33,r19=has_b12)
        assert info["fog_words_unchanged"] and info["stock_specular_material_lane_unchanged"]
        assert info["gamma_opcodes_replaced_by_nops"]==16
        assert info["ptde_c100_b12_1_applied"]==has_b12
        _,raw=op.words_and_offset(patched)
        x=op.else_first_lane(op.words_and_offset(src)[1],33)
        assert raw[x["exp"]:x["exp"]+5]==[
            0x05000036,0x00100072,9,0x00100246,1]
        assert raw[x["log"]:x["exp"]]==[0x0100003a]*16
        assert raw[x["mul"]+6:x["mul"]+8]==(
            [12,1] if has_b12 else [0,9])
        assert op.dxbc_digest(patched)==patched[4:20]
        # Reverting a candidate or changing a validated preimage must fail.
        broken=bytearray(src)
        _,words=op.words_and_offset(src)
        bad_at=x["gamma"]+6
        code_offset,_=op.words_and_offset(src)
        broken[code_offset+4*bad_at]^=0x01
        broken[4:20]=op.dxbc_digest(broken)
        try:
            op.patch(bytes(broken),33,r19=has_b12)
        except ValueError:pass
        else:raise AssertionError("gamma mutation was not rejected")
    try:
        op.patch(fake_dxbc(False),33,r19=True)
    except ValueError:pass
    else:raise AssertionError("missing b12 PTDE material CB was admitted")
    print("V262_TWO_PATH_DIFFUSE_OPERATOR_DXBC_AND_FOG_FAIL_OPEN_PASS")
if __name__=="__main__":run()
