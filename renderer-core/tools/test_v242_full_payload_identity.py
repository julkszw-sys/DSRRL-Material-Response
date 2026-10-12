from pathlib import Path
import importlib.util
p=Path(__file__).with_name('apply_v242_full_payload_identity.py')
s=importlib.util.spec_from_file_location('v242',p)
m=importlib.util.module_from_spec(s)
s.loader.exec_module(m)
root=Path(__file__).resolve().parents[2]
source=(root/m.PATH).read_text()
patched=m.patch(source,True)
assert patched==source.replace(m.OLD_SAME,m.NEW_SAME,1).replace(m.OLD_HASH,m.NEW_HASH,1)
assert m.patch(patched,True)==patched
for key in ('envdiffuse_a','envdiffuse_b','envdiffuse_linear_valid','phn_k135','phn_k135_valid'):
 assert 'source.'+key in m.NEW_HASH and 'a.'+key in m.NEW_SAME
assert 'return 43;' in m.NEW_TEST
print('V242_IDENTITY_SOURCE_TEST_PASS')
