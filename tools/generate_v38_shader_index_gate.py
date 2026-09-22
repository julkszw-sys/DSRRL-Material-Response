#!/usr/bin/env python3
from __future__ import annotations
import argparse
from pathlib import Path

OLD_GATE = 'if(r->kind!=receiver_kind::stock||r->id<33u||r->id>35u){++g_present_reject;++g_fail_open;return std::nullopt;}'
NEW_GATE = '''std::size_t owned_idx=3u;
        if(r->kind==receiver_kind::stock){
            if(r->id==894u) owned_idx=0u;
            else if(r->id==913u) owned_idx=1u;
            else if(r->id==932u) owned_idx=2u;
        }
        if(owned_idx>=3u){++g_present_reject;++g_fail_open;return std::nullopt;}'''
OLD_BIND = 'p.replacement_ps=g_owned_dedicated_ps[r->id-33u];'
NEW_BIND = 'p.replacement_ps=g_owned_dedicated_ps[owned_idx];'


def main():
    ap=argparse.ArgumentParser()
    ap.add_argument('--input',type=Path,required=True)
    ap.add_argument('--output',type=Path,required=True)
    a=ap.parse_args()
    text=a.input.read_text(encoding='utf-8')
    if OLD_GATE not in text: raise SystemExit('old V3.6 receiver gate not found')
    if OLD_BIND not in text: raise SystemExit('old V3.6 owned PS index not found')
    text=text.replace(OLD_GATE,NEW_GATE,1)
    text=text.replace(OLD_BIND,NEW_BIND,1)
    text=text.replace('V3.6 Owned Dedicated PS','V3.8 Shader-Index Gate')
    text=text.replace('[DSRRL FULL ENVSPEC V3.6]','[DSRRL FULL ENVSPEC V3.8]')
    text=text.replace('V3.6 owned dedicated P_Metal PS island armed','V3.8 exact shader-index P_Metal consumer island armed')
    text=text.replace('Exact build131 + exact P_Metal + exact stock 33/34/35 -> owned DXBC72/73/74 consumer island; raw PTDE RGBA; native PS+t12/t14+s12/s14 transaction; fail-open elsewhere.',
                      'Exact build131 + exact P_Metal + stock shader_index 894/913/932 -> owned DXBC72/73/74 consumer island; raw PTDE RGBA; native PS+t12/t14+s12/s14 transaction; fail-open elsewhere.')
    for tok in ('r->id==894u','r->id==913u','r->id==932u','g_owned_dedicated_ps[owned_idx]','V3.8 Shader-Index Gate'):
        if tok not in text: raise SystemExit('missing '+tok)
    if 'r->id<33u||r->id>35u' in text or 'r->id-33u' in text:
        raise SystemExit('legacy receiver-ordinal gate still present')
    a.output.parent.mkdir(parents=True,exist_ok=True)
    a.output.write_text(text,encoding='utf-8')
    print('PASS V3.8 shader-index gate 894/913/932 -> 72/73/74')

if __name__=='__main__': main()
