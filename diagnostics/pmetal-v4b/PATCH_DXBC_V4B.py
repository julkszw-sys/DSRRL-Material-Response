from pathlib import Path
import struct, sys, hashlib, json

SHIFTS = [
7,12,17,22,7,12,17,22,7,12,17,22,7,12,17,22,
5,9,14,20,5,9,14,20,5,9,14,20,5,9,14,20,
4,11,16,23,4,11,16,23,4,11,16,23,4,11,16,23,
6,10,15,21,6,10,15,21,6,10,15,21,6,10,15,21]
K=[
0xd76aa478,0xe8c7b756,0x242070db,0xc1bdceee,0xf57c0faf,0x4787c62a,0xa8304613,0xfd469501,
0x698098d8,0x8b44f7af,0xffff5bb1,0x895cd7be,0x6b901122,0xfd987193,0xa679438e,0x49b40821,
0xf61e2562,0xc040b340,0x265e5a51,0xe9b6c7aa,0xd62f105d,0x02441453,0xd8a1e681,0xe7d3fbc8,
0x21e1cde6,0xc33707d6,0xf4d50d87,0x455a14ed,0xa9e3e905,0xfcefa3f8,0x676f02d9,0x8d2a4c8a,
0xfffa3942,0x8771f681,0x6d9d6122,0xfde5380c,0xa4beea44,0x4bdecfa9,0xf6bb4b60,0xbebfbc70,
0x289b7ec6,0xeaa127fa,0xd4ef3085,0x04881d05,0xd9d4d039,0xe6db99e5,0x1fa27cf8,0xc4ac5665,
0xf4292244,0x432aff97,0xab9423a7,0xfc93a039,0x655b59c3,0x8f0ccc92,0xffeff47d,0x85845dd1,
0x6fa87e4f,0xfe2ce6e0,0xa3014314,0x4e0811a1,0xf7537e82,0xbd3af235,0x2ad7d2bb,0xeb86d391]
MASK=0xffffffff

def rol(x,s): return ((x<<s)|(x>>(32-s))) & MASK

def transform(state, block):
    words=struct.unpack('<16I',block)
    a,b,c,d=state
    for i in range(64):
        if i<16:
            f=(b&c)|((~b)&d); g=i
        elif i<32:
            f=(b&d)|(c&(~d)); g=(5*i+1)&15
        elif i<48:
            f=b^c^d; g=(3*i+5)&15
        else:
            f=c^(b|(~d)); g=(7*i)&15
        prevd=d; d=c; c=b
        b=(b + rol((a + (f&MASK) + K[i] + words[g]) & MASK, SHIFTS[i])) & MASK
        a=prevd
    state[0]=(state[0]+a)&MASK; state[1]=(state[1]+b)&MASK; state[2]=(state[2]+c)&MASK; state[3]=(state[3]+d)&MASK

def dxbc_checksum(blob: bytes)->bytes:
    assert blob[:4]==b'DXBC' and len(blob)>=20
    data=blob[20:]
    state=[0x67452301,0xefcdab89,0x98badcfe,0x10325476]
    full=len(data)&~63
    for off in range(0,full,64): transform(state,data[off:off+64])
    rem=data[full:]
    bitcount=(len(data)*8)&MASK
    block=bytearray(64)
    if len(rem)>=56:
        block[:len(rem)]=rem; block[len(rem)]=0x80
        transform(state,bytes(block)); block=bytearray(64)
        struct.pack_into('<I',block,0,bitcount)
    else:
        struct.pack_into('<I',block,0,bitcount)
        block[4:4+len(rem)]=rem; block[4+len(rem)]=0x80
    struct.pack_into('<I',block,60,((bitcount>>2)|1)&MASK)
    transform(state,bytes(block))
    return struct.pack('<4I',*state)

def validate(blob): return blob[4:20]==dxbc_checksum(blob)

def get_shex(blob):
    n=struct.unpack_from('<I',blob,28)[0]
    for k in range(n):
        o=struct.unpack_from('<I',blob,32+4*k)[0]
        tag=blob[o:o+4]; sz=struct.unpack_from('<I',blob,o+4)[0]
        if tag in (b'SHEX',b'SHDR'):
            return o+8, sz//4
    raise ValueError('no SHEX/SHDR')

POS={
 33: dict(prefog=2448, fogadd=2482, postpow=2686, terminal=2822),
 34: dict(prefog=2367, fogadd=2401, postpow=2605, terminal=2741),
 35: dict(prefog=2020, fogadd=2054, postpow=2258, terminal=2394),
}

PREF=[
  0x0600002f, 0x00100072,0x00000002, 0x00208246,0x00000000,0x0000000c,
  0x0a000038, 0x00100072,0x00000002, 0x00100246,0x00000002, 0x00004002,0x3ee8ba2f,0x3ee8ba2f,0x3ee8ba2f,0x00000000,
  0x05000019, 0x00100072,0x00000002, 0x00100246,0x00000002,
]
FOGADD=[0x09000000,0x00100072,0x00000002,0x80100246,0x00000041,0x00000001,0x80100246,0x00000081,0x00000002]
NOP=0x0100003a
POST=[NOP]*21
SAT_TOKEN=0x05002036

def patch(blob, idx):
    assert validate(blob)
    b=bytearray(blob); shex_off,nw=get_shex(blob)
    words=list(struct.unpack_from('<'+'I'*nw,b,shex_off))
    p=POS[idx]
    assert words[p['prefog']]==0x0600002f and words[p['prefog']+6]==0x0a000038 and words[p['prefog']+16]==0x05000019
    assert words[p['fogadd']]==0x09000000
    assert words[p['postpow']]==0x0600002f and words[p['postpow']+6]==0x0a000038 and words[p['postpow']+16]==0x05000019
    assert words[p['terminal']]==0x05000036
    words[p['prefog']:p['prefog']+21]=PREF
    words[p['fogadd']:p['fogadd']+9]=FOGADD
    words[p['postpow']:p['postpow']+21]=POST
    words[p['terminal']]=SAT_TOKEN
    struct.pack_into('<'+'I'*nw,b,shex_off,*words)
    b[4:20]=dxbc_checksum(bytes(b))
    assert validate(bytes(b))
    return bytes(b)
