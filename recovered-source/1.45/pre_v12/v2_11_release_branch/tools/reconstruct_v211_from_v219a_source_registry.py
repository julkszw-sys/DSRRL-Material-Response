from pathlib import Path
import re, struct, hashlib, zipfile
ROOT=Path('/mnt/data/mr_core_recovery')
Z=ROOT/'V219A.zip'
SRC=ROOT/'v211src/include/dsrrl/ptde_material_donor_registry.hpp'
OUT=ROOT/'DSRRL_PTDE_MATERIAL_RESPONSE_V2_11_reconstructed.addon64'
TARGET='1cfa2154059575d2dbf3f34124e7d51c4c63e8ab8f22347ae56755bc1e5f697a'
REG_OFF=0x81280; REC=0x48; N=368
IMAGE=0x180000000; RDATA_RAW=0xd400; RDATA_RVA=0xe000

def va_to_raw(va): return RDATA_RAW + ((va-IMAGE)-RDATA_RVA)
with zipfile.ZipFile(Z) as z:
    name=next(n for n in z.namelist() if n.endswith('.addon64'))
    base=z.read(name)
b=bytearray(base)
text=SRC.read_text(encoding='utf-8')
# Stable parse: each donor line begins with 64-hex SHA and ends with bool.
rows=[]
for ln in text.splitlines():
    m=re.search(r'^\s*\{"([0-9a-f]{64})".*?,\s*(true|false)\},\s*$',ln)
    if m: rows.append((m.group(1), m.group(2)=='true'))
assert len(rows)==N, len(rows)
# Restore exactly the only fields V2.19A mutates relative to V2.11:
# pointed-to hash string for all 368 records, and has_spec byte at +0x44.
for i,(sha,has) in enumerate(rows):
    off=REG_OFF+i*REC
    ptr,n=struct.unpack_from('<QQ',b,off)
    assert n==64
    ho=va_to_raw(ptr)
    b[ho:ho+64]=sha.encode('ascii')
    b[off+0x44]=1 if has else 0
OUT.write_bytes(b)
sha=hashlib.sha256(b).hexdigest()
print('input_v219a',hashlib.sha256(base).hexdigest())
print('output_v211',sha)
print('size',len(b))
print('target',TARGET)
print('PASS',sha==TARGET)
if sha!=TARGET:
    raise SystemExit(2)
