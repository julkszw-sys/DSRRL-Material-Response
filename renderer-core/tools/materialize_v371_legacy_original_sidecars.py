#!/usr/bin/env python3
"""V371: verify all original PTDE Classic EnvDiffuse DDS and materialize ONLY
m18_500/m18_510 exact RGBA8 cubes used by native-name/SHA gated runtime.
No heuristic DSR producer mapping for the other 37 resources.
"""
import argparse, hashlib, json, os, struct
from pathlib import Path

HEADER=struct.Struct('<8sIIQQ')
RECORD=struct.Struct('<40s6HIQQ32s24s')
OUT={
    'EnvDif_m18_500': ('EnvDif_m18_500_PTDE_RGBA8_CUBE.bin',
       'f1f6b147291e0648e219bf8c6475f74743d22c66b7f885fe547bdb98ab4542b4',32),
    'EnvDif_m18_510': ('EnvDif_m18_510_PTDE_RGBA8_CUBE.bin',
       '7c9af74457b4a488349e117c9eafafb66881142702186545f8906d43ca6c4426',8),
}
EXPECTED_PACK_SHA='3be05fffacfbc0af3f5a4124f0f79694c20480057b3a5c76470667ed712a6b47'

def audit_pack(pack,manifest):
    data=pack.read_bytes()
    records=json.loads(manifest.read_text(encoding='utf-8'))['records']
    if len(data)!=820208 or hashlib.sha256(data).hexdigest()!=EXPECTED_PACK_SHA:
        raise ValueError('PTDE DSRLEDF1 pack size/SHA mismatch')
    if HEADER.unpack_from(data)!=(b'DSRLEDF1',1,39,32,5024):
        raise ValueError('PTDE DSRLEDF1 header mismatch')
    if len(records)!=39:raise ValueError('Expected exactly 39 manifest records')
    seen=set()
    off=5024
    extracted={}
    for i in range(39):
        name_raw,map_id,row,w,h,mips,fmt,flags,origin,size,sha,pad=RECORD.unpack_from(data,32+i*128)
        name=name_raw.split(bytes([0]),1)[0].decode('ascii')
        if name in seen or not name.startswith('EnvDif_') or flags!=1 or any(pad):
            raise ValueError('Ambiguous or invalid source metadata '+name)
        seen.add(name)
        if origin!=off or size<=128 or origin+size>len(data):
            raise ValueError('Data overlap/bounds mismatch '+name)
        dds=data[origin:origin+size]
        off=origin+size
        if dds[:4]!=b'DDS ' or struct.unpack_from('<I',dds,4)[0]!=124:
            raise ValueError('Invalid original DDS header '+name)
        if struct.unpack_from('<II',dds,12)!=(h,w):
            raise ValueError('DDS dimensions mismatch '+name)
        if struct.unpack_from('<I',dds,28)[0]!=mips:
            raise ValueError('DDS mip count mismatch '+name)
        if struct.unpack_from('<I',dds,112)[0]&0xfe00!=0xfe00:
            raise ValueError('DDS cubemap faces mismatch '+name)
        digest=hashlib.sha256(dds).digest()
        if digest!=sha or records[i]['name']!=name or records[i]['dds_sha256']!=sha.hex():
            raise ValueError('Original DDS or manifest SHA mismatch '+name)
        if fmt==1:
            if mips!=1 or size!=128+w*h*3*6 or tuple(struct.unpack_from('<IIIII',dds,88))!=(24,0xff0000,0xff00,0xff,0):
                raise ValueError('RGB24 original format mismatch '+name)
        elif fmt==2:
            if dds[84:88]!=b'DXT1' or size!=128+6*sum(max(1,((w>>k)+3)//4)*max(1,((h>>k)+3)//4)*8 for k in range(mips)):
                raise ValueError('BC1 original mip/size mismatch '+name)
        else:
            raise ValueError('Unknown original DDS format '+name)
        if name in OUT:
            _,want_sha,edge=OUT[name]
            if fmt!=1 or mips!=1 or w!=edge or h!=edge:
                raise ValueError('m18 exact donor dimension/format mismatch')
            rgba=bytearray()
            raw=dds[128:]
            for k in range(0,len(raw),3):
                b,g,r=raw[k:k+3]
                rgba.extend((r,g,b,255))
            if hashlib.sha256(rgba).hexdigest()!=want_sha:
                raise ValueError('m18 RGBA original donor SHA mismatch')
            extracted[name]=bytes(rgba)
    if off!=len(data) or len(seen)!=39 or len(extracted)!=2:
        raise ValueError('PTDE pack incomplete or trailing data')
    return extracted

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--sidecar',type=Path,required=True)
    ap.add_argument('--manifest',type=Path,required=True)
    ap.add_argument('--game-root',type=Path,help='Write exact m18 donor cubes here (default: verify only)')
    args=ap.parse_args()
    donors=audit_pack(args.sidecar,args.manifest)
    if args.game_root:
        target=args.game_root/'DSRRL'/'EnvSpec'/'LegacyGI'
        target.mkdir(parents=True,exist_ok=True)
        for name,data in donors.items():
            path=target/OUT[name][0]
            temp=path.with_suffix('.tmp')
            temp.write_bytes(data)
            os.replace(temp,path)
    print('V371 DSRLEDF1 PASS: 39/39 original PTDE DDS verified; 2 exact m18 RGBA donors; other 37 UNROUTED')
    if args.game_root:print('Two donor bins installed; only exact native t11 name+creation SHA+COM view can bind them.')

if __name__=='__main__':
    main()
