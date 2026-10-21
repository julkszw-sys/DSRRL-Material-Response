#!/usr/bin/env python3
"""Exact DSR GI_Reflection selector5 source-name -> canonical GI probe ordinal.

This patches the V359 resource-name matcher only AFTER V359's source and
view-scope materializers. NO unknown SHA whitelist and NO name heuristic:
a real retail producer name, COM-exact resource backing, two independent
known SHA/name anchors in the SAME MAP, same material+receiver, and existing
atomic draw are still required. Unnamed SHA remains stock DSR fail-open.

Proof: owner DSR exe SHA256 a45aaa36..., RVA 0x2017c9..0x201823:
selector5 and selectors1..4 use the identical r12 mapstem+r15 packed probe,
differing solely in "GI_Reflection_" vs "GI_EnvSpc_" name family/slot.
"""
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
TARGET=ROOT/"src/runtime/envspec_resource_runtime.cpp"
MARKER="// V230 GI_Reflection selector5 -> 342 same-packed-ID source-name bridge"
START="bool v359_match_original_gi_name("
END="bool v359_verified_native_name_probe_locked("
BLOCK=r"""
#if defined(DSRRL_V230_REFLECTION_TO_342)
    // Original retail DSR selector 5 uses the SAME mapstem and packed ID
    // as GI_EnvSpc selectors 1..4. Compare the FULL source name bytewise.
    constexpr wchar_t reflect_prefix[]=L"GI_Reflection_";
    constexpr wchar_t env_prefix[]=L"GI_EnvSpc_";
    constexpr std::size_t reflect_len=sizeof(reflect_prefix)/sizeof(wchar_t)-1u;
    constexpr std::size_t env_len=sizeof(env_prefix)/sizeof(wchar_t)-1u;
    if(length>=reflect_len+1u) {
        bool prefix_ok=true;
        for(std::size_t j=0u;j<reflect_len;++j)
            if(name[j]!=reflect_prefix[j]) { prefix_ok=false; break; }
        if(prefix_ok) {
            for(const auto &r:operators::env_spec::generated::k_v359_names) {
                const wchar_t *src=r.name;
                bool env_ok=true;
                for(std::size_t j=0u;j<env_len;++j)
                    if(src[j]!=env_prefix[j]) { env_ok=false; break; }
                if(!env_ok) continue;
                std::size_t src_len=0u;
                while(src_len<64u && src[src_len]!=L'\0')++src_len;
                // V359 certified name: GI_EnvSpc_mXX_YY_ZZZZ_0
                if(src_len<env_len+3u || src_len>=64u ||
                   src[src_len-2u]!=L'_' || src[src_len-1u]!=L'0')
                    continue;
                const auto stem_len=src_len-env_len-2u;
                // Selector5 name: GI_Reflection_mXX_YY_ZZZZ
                if(length!=reflect_len+stem_len) continue;
                bool identical=true;
                for(std::size_t j=0u;j<stem_len;++j)
                    if(name[reflect_len+j]!=src[env_len+j]) {
                        identical=false; break;
                    }
                if(!identical) continue;
                ordinal=r.ordinal;
                map_id=r.map_id;
                return map_id<20u;
            }
        }
    }
#endif
"""

def transform(src):
    if MARKER in src:
        raise RuntimeError("Already patched: refuse duplicate alias")
    if src.count(START)!=1 or src.count(END)!=1:
        raise RuntimeError("Missing exact V359 producer/consumer context")
    start=src.index(START)
    end=src.index(END,start)
    fragment=src[start:end]
    anchor="    return false;\n}\n"
    if fragment.count(anchor)!=1 or "k_v359_names" not in fragment:
        raise RuntimeError("V359 exact name matcher drift")
    fragment=fragment.replace(anchor,MARKER+"\n"+BLOCK+anchor,1)
    return src[:start]+fragment+src[end:]

if __name__=="__main__":
    before=TARGET.read_text("utf8")
    after=transform(before)
    TARGET.write_text(after,"utf8")
    print("GI_REFLECTION_342_EXACT_NAME_SOURCE_PATCH_PASS; 15 unknown SHA remain fail-open until source-name backing is attested")
