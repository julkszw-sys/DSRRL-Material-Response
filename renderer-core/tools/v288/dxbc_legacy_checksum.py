"""Checksum for legacy DXBC containers, not standard MD5.
Matches the project's C++ dxbc_checksum.hpp implementation."""
import math
import struct

MASK=0xffffffff
ROT=([7,12,17,22]*4+[5,9,14,20]*4+[4,11,16,23]*4+[6,10,15,21]*4)
K=[int(abs(math.sin(i+1))*2**32)&MASK for i in range(64)]
INIT=(0x67452301,0xefcdab89,0x98badcfe,0x10325476)

def block(state,data):
    w=struct.unpack('<16I',data)
    a,b,c,d=state
    aa,bb,cc,dd=state
    for i in range(64):
        if i<16:f=(b&c)|((~b)&d);g=i
        elif i<32:f=(d&b)|((~d)&c);g=(5*i+1)%16
        elif i<48:f=b^c^d;g=(3*i+5)%16
        else:f=c^(b|(~d));g=(7*i)%16
        t=(a+(f&MASK)+K[i]+w[g])&MASK
        n=((t<<ROT[i])|(t>>(32-ROT[i])))&MASK
        a,b,c,d=d,(b+n)&MASK,b,c
    return tuple((x+y)&MASK for x,y in zip((aa,bb,cc,dd),(a,b,c,d)))

def digest(data):
    if len(data)<32 or data[:4]!=b'DXBC' or struct.unpack_from('<I',data,24)[0]!=len(data):
        raise ValueError('invalid DXBC envelope')
    payload=data[20:]
    bits=len(payload)*8
    n=len(payload)&~63
    state=INIT
    for off in range(0,n,64):state=block(state,payload[off:off+64])
    remain=payload[n:]
    buf=bytearray(64)
    if len(remain)>=56:
        buf[:len(remain)]=remain
        buf[len(remain)]=0x80
        state=block(state,buf)
        buf=bytearray(64)
        struct.pack_into('<I',buf,0,bits&MASK)
        struct.pack_into('<I',buf,60,((bits>>2)|1)&MASK)
    else:
        struct.pack_into('<I',buf,0,bits&MASK)
        buf[4:4+len(remain)]=remain
        buf[4+len(remain)]=0x80
        struct.pack_into('<I',buf,60,((bits>>2)|1)&MASK)
    return struct.pack('<4I',*block(state,buf))

def valid(data):
    try:return data[4:20]==digest(data)
    except (ValueError,struct.error):return False

def repair(data):
    return data[:4]+digest(data)+data[20:]
