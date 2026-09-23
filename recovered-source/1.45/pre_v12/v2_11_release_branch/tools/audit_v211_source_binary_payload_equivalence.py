from pathlib import Path
import re,hashlib,json,struct
ROOT=Path('/mnt/data/mr_core_recovery')
BIN=(ROOT/'DSRRL_PTDE_MATERIAL_RESPONSE_V2_11_reconstructed.addon64').read_bytes()
HEADERS=[
 ('c101',ROOT/'v211src/include/dsrrl/generated_ptde_c101_material_response.hpp'),
 ('diffuse',ROOT/'v211src/include/dsrrl/generated_ptde_diffuse_material_response.hpp'),
]
res={'binary_sha256':hashlib.sha256(BIN).hexdigest(),'headers':{},'all_payloads_found_exactly_once':True}
for label,p in HEADERS:
 t=p.read_text()
 arr=[]
 pat=re.compile(r'inline constexpr std::array<std::uint8_t,(\d+)>\s+(blob_\d+)\s*=\s*\{(.*?)\n\};',re.S)
 for n,name,body in pat.findall(t):
  nums=[int(x) for x in re.findall(r'\b\d+\b',body)]
  assert len(nums)==int(n),(label,name,len(nums),n)
  blob=bytes(nums); pos=[]; st=0
  while True:
   q=BIN.find(blob,st)
   if q<0: break
   pos.append(q); st=q+1
  arr.append({'name':name,'size':len(blob),'sha256':hashlib.sha256(blob).hexdigest(),'positions':[hex(x) for x in pos],'count':len(pos)})
  if len(pos)!=1: res['all_payloads_found_exactly_once']=False
 res['headers'][label]={'file_sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'count':len(arr),'payloads':arr}
# donor registry string order / record fields validation
reg=(ROOT/'v211src/include/dsrrl/ptde_material_donor_registry.hpp').read_text()
rows=[]
for ln in reg.splitlines():
 m=re.search(r'^\s*\{"([0-9a-f]{64})", \{([^}]*)\}, (\d+), \{([^}]*)\}, \{([^}]*)\}, (\d+), ([^,]+), (-?\d+), (true|false)\},',ln)
 if m: rows.append(m.groups())
assert len(rows)==368
REG_OFF=0x81280; REC=0x48; IMAGE=0x180000000; RDATA_RAW=0xd400; RDATA_RVA=0xe000
def va_to_raw(va): return RDATA_RAW+((va-IMAGE)-RDATA_RVA)
registry_ok=True; problems=[]
for i,r in enumerate(rows):
 sha=r[0]; off=REG_OFF+i*REC; ptr,n=struct.unpack_from('<QQ',BIN,off); ho=va_to_raw(ptr)
 got=BIN[ho:ho+64].decode('ascii')
 if got!=sha: registry_ok=False; problems.append((i,'sha',sha,got))
 has=(r[-1]=='true'); got_has=bool(BIN[off+0x44])
 if got_has!=has: registry_ok=False; problems.append((i,'has_spec',has,got_has))
res['registry']={'rows':len(rows),'source_sha256':hashlib.sha256((ROOT/'v211src/include/dsrrl/ptde_material_donor_registry.hpp').read_bytes()).hexdigest(),'hash_and_has_spec_exact':registry_ok,'problems':problems[:20]}
res['pass']=res['all_payloads_found_exactly_once'] and registry_ok and res['binary_sha256']=='1cfa2154059575d2dbf3f34124e7d51c4c63e8ab8f22347ae56755bc1e5f697a'
out=ROOT/'V211_SOURCE_BINARY_PAYLOAD_EQUIVALENCE.json';out.write_text(json.dumps(res,indent=2))
print(json.dumps({
 'pass':res['pass'],'binary':res['binary_sha256'],
 'c101_count':res['headers']['c101']['count'],'diffuse_count':res['headers']['diffuse']['count'],
 'all_payloads_once':res['all_payloads_found_exactly_once'],'registry_ok':registry_ok,
 'audit_sha256':hashlib.sha256(out.read_bytes()).hexdigest()
},indent=2))
