#!/usr/bin/env python3
"""Audit and materialize strictly source-proven BASE PTDE armor sidecar tuple supplement.

552 physical same-binder (Diffuse, Specular, optional Normal) PTDE source tuples
in original owner sidecarnew_v5.zip SHA256
a96f8c70d1cafe018f4c0f6db2d331c9d265c86e4b1aa8c9a85476c1b361dc6b.
548 exact distinct Specular logical identities; 99% default validated D=s[:-2],
N=s[:-2]+'_n'. Exceptions explicitly enumerated; ambiguous LG_F_9400 source
normal conflict fails open. Does NOT include _L/_M, FC, or unrelated MTDs.
Only additional tuples absent from v12 but present in recovered DSR SpecRGB names
are emitted; positive activation ALSO requires live exact owner/MTD/RX/t0/t1/t2.
No blanket shader/PBL or asset patch.
"""
from pathlib import Path
import base64, zlib, json, re, hashlib

SPEC_CORPUS_B64 = """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"""
EXCEPTIONS = {
  'bd_f_2540_bd_m_2540_s': ('bd_f_2540_bd_m_2540', None),
  'bd_m_2540_s': ('bd_m_2540', None),
  'bd_m_9450_1_s': ('bd_m_9450_1', None),
}
# LG_F_9400 uses two incompatible PTDE normal paths with the same spec
# logical name. Exclude from canonical supplement until exact DSR mapping.
AMBIGUOUS = {'lg_f_9400_lg_m_9400_s'}

def fnv(s):
    h=0xcbf29ce484222325
    for c in s.casefold().encode('utf-8'):
        h=((h^c)*0x100000001b3)&0xffffffffffffffff
    return h

def fail(msg):
    raise SystemExit('ARMOR_V13_FAIL_CLOSED: '+msg)

ROOT = Path('renderer-core')
INC = ROOT / 'include/dsrrl/runtime'
srcdiff = (INC/'generated_diffuse_routes_v12.hpp').read_text('utf-8')
srcnorm = (INC/'generated_normal_routes_v12.hpp').read_text('utf-8')
specsource = (ROOT/'data/provenance/spec_routes_v12.tsv').read_text('utf-8')

pair_pat = re.compile(r'\{(0x[0-9a-f]{16})ull,\s*(0x[0-9a-f]{16})ull\}', re.I)
triple_pat = re.compile(r'\{(0x[0-9a-f]{16})ull,\s*(0x[0-9a-f]{16})ull,\s*(0x[0-9a-f]{16})ull\}', re.I)
original_pairs = {(int(a,16),int(b,16)) for a,b in pair_pat.findall(srcdiff)}
original_triples = {(int(a,16),int(b,16),int(c,16)) for a,b,c in triple_pat.findall(srcnorm)}
if len(original_pairs)!=528 or len(original_triples)!=557:
    fail(f'v12 source drift pairs={len(original_pairs)} triples={len(original_triples)}')
stock_spec_names = {r.split('\t')[-1].strip().casefold()
   for r in specsource.splitlines() if r and not r.startswith('#') and '\t' in r}
corpus = json.loads(zlib.decompress(base64.b64decode(SPEC_CORPUS_B64)))
if len(corpus)!=548 or len(set(corpus))!=548:
    fail(f'PTDE source cardinality drift {len(corpus)}')
if any(not s.endswith('_s') or '_l_' in s for s in corpus):
    fail('spec names not BASE candidates')
candidate_pairs=set()
candidate_triples=set()
not_in_stock_spec=[]
for spec in corpus:
    if spec in AMBIGUOUS: continue
    if spec not in stock_spec_names:
        not_in_stock_spec.append(spec);continue
    diffuse,normal=EXCEPTIONS.get(spec,(spec[:-2],spec[:-2]+'_n'))
    candidate_pairs.add((fnv(spec),fnv(diffuse)))
    if normal:
        candidate_triples.add((fnv(diffuse),fnv(spec),fnv(normal)))
newpairs=sorted(candidate_pairs-original_pairs)
newtriples=sorted(candidate_triples-original_triples)
if not newpairs and not newtriples: fail('No missing PTDE armor material tuples; invalid input or already patched')
# No candidate may authorize a different DSR spec than an attested stock logical id.
allowed_diffuse=sorted(set(b for _,b in newpairs))
allowed_normal=sorted(set(c for _,_,c in newtriples))
def lit(n):return '0x%016xull'%n
def arr(rows,names):return ',\n'.join('  {{'+','.join(map(lit,row))+'}}' for row in rows)
header = '''#pragma once
#include <array>
#include <cstdint>
namespace dsrrl::runtime::armor_v13 {
struct pair {std::uint64_t s,d;};
struct triple {std::uint64_t d,s,n;};
template<typename T,std::size_t N> constexpr bool contains(const std::array<T,N>&a,std::uint64_t key) {
 for (const auto &v:a) if(v==key)return true;return false;
}
inline constexpr std::array<pair,%d> p = {{
%s
}};
inline constexpr std::array<triple,%d> t = {{
%s
}};
inline constexpr std::array<std::uint64_t,%d> d = {{%s}};
inline constexpr std::array<std::uint64_t,%d> n = {{%s}};
inline constexpr bool pair_ok(std::uint64_t s,std::uint64_t d0) {
 for(const auto &x:p)if(x.s==s && x.d==d0)return true;return false;
}
inline constexpr bool triple_ok(std::uint64_t d0,std::uint64_t s,std::uint64_t n0) {
 for(const auto &x:t)if(x.d==d0 && x.s==s && x.n==n0)return true;return false;
}
inline constexpr bool diffuse_name(std::uint64_t h){return contains(d,h);}
inline constexpr bool normal_name(std::uint64_t h){return contains(n,h);}
}
'''%(len(newpairs),arr(newpairs,2),len(newtriples),arr(newtriples,3),len(allowed_diffuse),','.join(map(lit,allowed_diffuse)),len(allowed_normal),','.join(map(lit,allowed_normal)))
header_path=INC/'generated_armor_v13_base_exact.hpp'
header_path.write_text(header,encoding='utf-8')
cpp=ROOT/'src/runtime/material_resource_draw_runtime.cpp'
text=cpp.read_text(encoding='utf-8')
def patch(old,new):
 global text
 if text.count(old)!=1:fail('C++ anchor drift count='+str(text.count(old))+' at '+old[:80])
 text=text.replace(old,new,1)
patch('#include "dsrrl/runtime/generated_normal_routes_v12.hpp"',
      '#include "dsrrl/runtime/generated_normal_routes_v12.hpp"\n#include "dsrrl/runtime/generated_armor_v13_base_exact.hpp"')
# Only exact native stock t0/t1/t2 and certified live DSR FLVER/slot/MTD.
patch('    const auto h2 = hashes[2];\n\n    if (full_material_response_ready &&',
'''    const auto h2 = hashes[2];
    const bool v13_owner =
        query.material.owner_tuple_exact &&
        query.material.material_slot_valid &&
        operators::material_response::
            has_exact_flver_material_ownership(query) &&
        receiver_id >= 24u && receiver_id <= 35u;
    const bool v13_pair =
        v13_owner && armor_v13::pair_ok(h1,h0);
    const bool v13_triple =
        v13_owner && armor_v13::triple_ok(h0,h1,h2);
    if (full_material_response_ready &&''')
patch('generated::diffuse_pair_allowed_v12(\n            h1,\n            h0);',
      '(generated::diffuse_pair_allowed_v12(h1,h0) || v13_pair);')
patch('generated::normal_tuple_allowed_v12(\n            h0,\n            h1,\n            h2);',
      '(generated::normal_tuple_allowed_v12(h0,h1,h2) || v13_triple);')
patch('generated::diffuse_target_hash_allowed_v12(hash)\n                      : generated::normal_target_hash_allowed_v12(hash);',
      '(generated::diffuse_target_hash_allowed_v12(hash) || armor_v13::diffuse_name(hash))\n                      : (generated::normal_target_hash_allowed_v12(hash) || armor_v13::normal_name(hash));')
patch('generated::diffuse_name_hash_allowed_v12(\n            logical_hash);',
      '(generated::diffuse_name_hash_allowed_v12(logical_hash) || armor_v13::diffuse_name(logical_hash));')
patch('generated::normal_name_hash_allowed_v12(\n            logical_hash);',
      '(generated::normal_name_hash_allowed_v12(logical_hash) || armor_v13::normal_name(logical_hash));')
cpp.write_text(text,encoding='utf-8')
report={"source":"PTDE physical same-binder BASE source; DSR spec v12 actual name corpus",
"ptde_triples_original":552,"ptde_spec_unique":548,"v12_pairs":528,"v12_triples":557,
"ptde_candidate_pairs":len(candidate_pairs),"ptde_candidate_triples":len(candidate_triples),
"missing_pairs_added":len(newpairs),"missing_triples_added":len(newtriples),
"source_spec_not_in_dsr_corpus":not_in_stock_spec,"ambiguous_source_names":sorted(AMBIGUOUS),
"v13_header_sha256":hashlib.sha256(header.encode()).hexdigest(),
"routing_scope":"ONLY base/source-tpf-certified pairs, known DSR t1 name, exact DSR FLVER/slot/MTD owner, RX24-35 and operator-local semantic use; runtime and pixels OPEN"}
out=ROOT/'data/census/armor_v13_20261023_report.json'
out.parent.mkdir(parents=True,exist_ok=True)
out.write_text(json.dumps(report,indent=2)+"\n",encoding='utf-8')
print(json.dumps(report,indent=2))
