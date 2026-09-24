#!/usr/bin/env python3
from __future__ import annotations
import argparse, json
from pathlib import Path

SITES = {
0:((1527,1617),1631),1:((1436,1526),1540),2:((1092,1182),1196),3:((1465,1555),1569),
4:((1374,1464),1478),5:((1034,1124),1138),6:((1254,1344),1358),7:((1163,1253),1267),
8:((819,909),923),9:((1195,1285),1299),10:((1104,1194),1208),11:((764,854),868),
12:((1083,1173),1187),13:((992,1082),1096),14:((648,738),752),15:((1021,1111),1125),
16:((930,1020),1034),17:((590,680),694),18:((997,1087),1101),19:((906,996),1010),
20:((562,652),666),21:((938,1028),1042),22:((847,937),951),23:((507,597),611),
}

# Zero-based SHEX word sites, validated 24/24 against the exact stock DSR binder.
# Each host has one exact t1 resource declaration and one exact specular t1 sample.
SPEC_SITES = {
0:(57,1364),1:(57,1273),2:(54,929),3:(54,1302),4:(54,1211),5:(51,871),
6:(48,1119),7:(48,1028),8:(45,684),9:(45,1060),10:(45,969),11:(42,629),
12:(51,920),13:(51,829),14:(48,485),15:(48,858),16:(48,767),17:(45,427),
18:(45,862),19:(45,771),20:(42,427),21:(42,803),22:(42,712),23:(39,372),
}

# Deterministic output identities for the RDEF-correct split-resource patch.
# The patch preserves stock t1 RGBA, then overwrites RGB only from t10.
SPEC_HASHES = {
0:("69750d5df5b5469a7f22a19cbafdd7cbb9449bc14c1f9fb25c5c0c5b9b85cd73","cedac61b82ed32c5a6b027a08bdaf85b96585faa2bcc1d842966c68076a83dbe"),
1:("34bdbe3cd1bd691dfae09f352c85e3dbf5d79e05b4c20cced6ca5755dfe49b18","2703d7b689b1dd46f6fcad764d96b766259887667648794eb5172fd61329708d"),
2:("fe6d7470be8f828cf3f90d2151a8472229435bf6228875ef1ec3cedda178022a","d46d9bc9b8b9219d2efb92ce0e17316da91b1a15204c4ea61590dc2005495e65"),
3:("e770f809a783a5c50de1a1e6b71cb5bb20bffe4ac95f99f9e09eae7484379e7f","1aeb06831e8e84073decb5edf14ea8f75d15952ebbc30a7802d4a5cacff92eee"),
4:("4fc77ed9bbc604299aed8d3953041f673ad069e54e7deb2e86af1f3cf0b413db","baaa211a99e1d69c741526a6fdc03ad42811b1c384a10721e5bd8536fc5f1b83"),
5:("1c86c927dee2738f29b7917340a6dc75c8f337082acb40440b08df9d77380414","a04f579ac75d39d27d25d39d058c78b4ced4bde94d0b63480cf3af6a5447a2f9"),
6:("87e7bc5006b6746bff5d9745a62127ed53582c1850286c0b032783b8dedf839e","34e4d29eeaf3015c17e7edd09139f0302d1cc9bbcff253311cd26e31e0de0732"),
7:("f2d819daef521fccdaa91544bfc62710e9b25bb4d3b3cc826af98f4b77137f5d","c127b027b366b087d44c596fa945df52c08b55317480abcb9a3f73a228f739c8"),
8:("761e4e2b4d375955ae8c173a7a6a990c75a1b62bde0f983221c3556f21e542e3","f215686b31c1f7745274fe7c913c3efd8f9e2b514fe7086aea71a0e9088d2b67"),
9:("d8fb6022cdcca896dfdb0fd4b9d1f4431b2c79d3039ef2a400ea45bdf02c606d","68ac8cf2fde58d12da6c8b118d8e6e28b9872038b6813fcc37cede946afb63b2"),
10:("e63b5734e90920f45f29fcbe3a55ee73f15fc202bae0af7d84180626914f4b17","dd636038c5bf23825e78207d5a157d583f3eeeb9d4f787d665c81e1def7ac171"),
11:("0b4615bd296253d754a8b6054a5df1a0b7609507bde95b9ba21dea0032cb6195","679ee198e629025503dae1708546a37a6bf7b02cd3a51b9a296e52511fa4e049"),
12:("583ee3d194f928da6368f4bf4add7d628c6e6e5a4d543256cf24c6ff3ccd98ca","44a1aae179ea700c76434d93e02ba8f5b99cb63e4396acee08215cbcd5b890a3"),
13:("2218616c91fc4f7810fa528338693a81792cc5130a92c2037e9ed5153af2d803","cd7734de9b490b9143fbaef3963a7519393c3a48f216580e458bb3333dd3715a"),
14:("ca0d54f901be42fa981b4bd84a16721df883c504d30e0483887a95bc66a1f779","c170ac061a8bfe1bd4c581ed2a8b9049c0e1a54dfd164e8b67d75b0777b93e4a"),
15:("12e776507e52a988bc5bf0860900a198bb898cc89266ab0faba0407bc5536309","ccc96de334dd1811d34da4d288ddddc5da3d060400ea1b6a5b18c35aa00f1abf"),
16:("be4108ade713dbce66fc20e66f72556951c8459a379d9b00a466221cee763ce0","9bd17e0ede95357b88a87ece72ca9a16430c11da325316a722f22670776c6bf9"),
17:("02f1d33d079e4a24c752368d7953d09b7521d6bfcc78d2834c09af2b8b11190f","6f3881fb2e738773c850427f25b4a2c6423fa1a05c2759673b4a9291a77a84bc"),
18:("fcf89f5d8455d77516f67179fd83d8db05d30a6da17d67f5bff04b8024f4fd46","ed126c555a5700025a026a063288a6bf7c36137e4a9e6584c66bce9038d6886d"),
19:("c3fd7d82708b196a1f559f2bd3907170e42de6037873ee6e6a73da448e714b30","149da456b54110c27a5cc54d24efd1df4be3955a4442083fb47824e9357f0c3e"),
20:("d0ece8e47bb0e51ce44b76b5a1e830431189afb7e0eb2d2cb6c778f0517071c5","12d2e2e063b5900852c739c69a0987556240e8a5bede951dd8ccfea63b1da997"),
21:("a5d4875867719470dca10b546fffd8425f76db1164c23bb7041d3937b770db04","36cd8d6d56cc3556cde6b16f664d5d0a07d4ed4065a4450d013b78fc177ce17c"),
22:("abb6381003d5072c699519a74cdfc1c7976256e1c2871fc008d773697177647f","7ada75ffc2d87de8e307b90c20dd249cf2c67426ac90b0ea1f3c2f0ca15cff86"),
23:("36e4b4e0efc30ce4e24b2f90fa6dcca7f212efeb272538b1f6638a05618126c4","6cc35f29cf05fbdff84274ca16fa79987b86d56040a0facbcfddad1f4cb88ae7"),
}

# Exact stock-DXR RDEF texture-binding counts for the 24 stable FULL24 hosts.
# SpecRGB appends: 20-byte name + duplicated binding table with one extra 32-byte
# t10 record + 60-byte SHEX delta, therefore growth = 112 + 32 * binding_count.
SPEC_RDEF_BINDING_COUNTS = {
0:31,1:31,2:29,3:29,4:29,5:27,6:25,7:25,8:23,9:23,10:23,11:21,
12:27,13:27,14:25,15:25,16:25,17:23,18:23,19:23,20:21,21:21,22:21,23:19,
}

def q(s: str) -> str:
    return '"' + s.replace('\\','\\\\').replace('"','\\"') + '"'

def main() -> int:
    ap=argparse.ArgumentParser()
    ap.add_argument('--v29',required=True); ap.add_argument('--v210',required=True)
    ap.add_argument('--v211',required=True); ap.add_argument('--out',required=True)
    a=ap.parse_args()
    v29=json.loads(Path(a.v29).read_text(encoding='utf-8'))
    v210=json.loads(Path(a.v210).read_text(encoding='utf-8'))['shader']['hosts']
    v211=json.loads(Path(a.v211).read_text(encoding='utf-8'))['hosts']
    stable=sorted((r for r in v29['records'] if not r['lerp']),key=lambda r:r['stable_index'])
    if len(stable)!=24 or len(v210)!=24 or len(v211)!=24: raise SystemExit('expected 24 stable hosts')
    lines=[
      '#pragma once\n#include <array>\n#include <cstddef>\n#include <cstdint>\n#include <string_view>\n\n',
      'namespace dsrrl::runtime::mr {\n',
      'struct word_patch { std::uint32_t word, old_value, new_value; };\n',
      'struct plan { std::uint8_t index; std::string_view label, original_sha256; std::uint32_t stock_size; ',
      'std::array<std::uint32_t,2> cb_sites; std::uint32_t pow_site; std::string_view v29_sha256; ',
      'std::array<word_patch,2> v210; std::string_view v210_sha256; word_patch v211; ',
      'std::string_view v211_sha256; std::uint32_t replacement_size; ',
      'std::uint32_t spec_dcl_t1_word, spec_sample_t1_word; ',
      'std::string_view v29_specrgb_sha256, v211_specrgb_sha256; std::uint32_t specrgb_replacement_size; };\n',
      'inline constexpr std::array<plan,24> k_plans = {{\n'
    ]
    for r in stable:
      i=r['stable_index']; h210=v210[i]; h211=v211[i]; cb,pow_site=SITES[i]
      p=h210['shex_patch_dwords']; sdcl,ssample=SPEC_SITES[i]; sh29,sh211=SPEC_HASHES[i]
      lines.append(
        '  {'+','.join([
          f'{i}u',q(r['label']),q(r['original_sha256']),f"{r['stock_size']}u",
          f'{{{{{cb[0]}u,{cb[1]}u}}}}',f'{pow_site}u',q(r['replacement_sha256']),
          f"{{{{{{{p[0]['word']}u,{int(p[0]['old'],16)}u,{int(p[0]['new'],16)}u}},{{{p[1]['word']}u,{int(p[1]['old'],16)}u,{int(p[1]['new'],16)}u}}}}}}",
          q(h210['c101_sha256']),
          f"{{{h211['chain_mul_word']}u,{int(h211['old_instruction_token'],16)}u,{int(h211['new_instruction_token'],16)}u}}",
          q(h211['v211_sha256']),f"{h211['size']}u",
          f'{sdcl}u',f'{ssample}u',q(sh29),q(sh211),f"{h211['size'] + 112 + 32*SPEC_RDEF_BINDING_COUNTS[i]}u"
        ])+'},\n'
      )
    lines += [
      '}};\n',
      'inline constexpr std::string_view k_stock_binder_sha256 = "ad180732ac79d5d98783aa504c789c2bab15e515c3b0f66b237d8b8c69113394";\n',
      '} // namespace dsrrl::runtime::mr\n'
    ]
    Path(a.out).write_text(''.join(lines),encoding='utf-8')
    return 0
if __name__=='__main__': raise SystemExit(main())
