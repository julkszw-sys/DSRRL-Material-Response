from pathlib import Path
import re,struct,hashlib,json
BASE=Path('v219a/DSRRL_PTDE_MATERIAL_RESPONSE_V2_19A_CE18_DIFFUSE_GATE.addon64')
HDR=Path('v211src/include/dsrrl/ptde_material_donor_registry.hpp')
OUT=Path('V211_reconstructed.addon64')
TARGET='1cfa2154059575d2dbf3f34124e7d51c4c63e8ab8f22347ae56755bc1e5f697a'
text=HDR.read_text(encoding='utf-8')
# exact 368 donor rows, final bool is source has_c101 / binary has_spec
rows=[]
pat=re.compile(r'^\s*\{"([0-9a-f]{64})",\s*\{[^\n]+?\},\s*(true|false)\},?\s*$',re.M)
for m in pat.finditer(text):
    rows.append((m.group(1), m.group(2)=='true'))
if len(rows)!=368:
    # less fragile fallback: capture first sha and final bool for every initializer line
    rows=[]
    for line in text.splitlines():
        m=re.match(r'^\s*\{"([0-9a-f]{64})".*\b(true|false)\},?\s*$',line)
        if m: rows.append((m.group(1),m.group(2)=='true'))
assert len(rows)==368, len(rows)
b=bytearray(BASE.read_bytes())
REG_OFF=0x81280; REC=0x48; N=368
IMAGE=0x180000000; RDATA_RAW=0xd400; RDATA_RVA=0xe000
def va_to_raw(va): return RDATA_RAW+((va-IMAGE)-RDATA_RVA)
for i,(sha,has) in enumerate(rows):
    off=REG_OFF+i*REC
    ptr,n=struct.unpack_from('<QQ',b,off)
    assert n==64
    ho=va_to_raw(ptr)
    b[ho:ho+64]=sha.encode('ascii')
    b[off+0x44]=1 if has else 0
OUT.write_bytes(b)
sha=hashlib.sha256(b).hexdigest()
print(json.dumps({'rows':len(rows),'sha256':sha,'target':TARGET,'exact':sha==TARGET,'size':len(b)},indent=2))
assert sha==TARGET
