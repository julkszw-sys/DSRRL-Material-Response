#!/usr/bin/env python3
from __future__ import annotations
import argparse
from pathlib import Path

DECL = r'''
std::array<std::atomic<std::uint64_t>,24> g_pmetal_receiver_census{};
std::atomic<std::uint64_t> g_pmetal_receiver_other{0};
'''.strip()

CENSUS = r'''
        if(r->kind==receiver_kind::stock&&r->id>=24u&&r->id<=47u)++g_pmetal_receiver_census[r->id-24u];
        else ++g_pmetal_receiver_other;
        ++g_present_reject;++g_fail_open;return std::nullopt;
'''.strip()

LOG = r'''
    char census[1500]{};
    std::snprintf(census,sizeof(census),
        "[DSRRL PMETAL RECEIVER CENSUS V3.7] r24=%llu r25=%llu r26=%llu r27=%llu r28=%llu r29=%llu r30=%llu r31=%llu r32=%llu r33=%llu r34=%llu r35=%llu r36=%llu r37=%llu r38=%llu r39=%llu r40=%llu r41=%llu r42=%llu r43=%llu r44=%llu r45=%llu r46=%llu r47=%llu other=%llu",
        static_cast<unsigned long long>(g_pmetal_receiver_census[0].load()),static_cast<unsigned long long>(g_pmetal_receiver_census[1].load()),static_cast<unsigned long long>(g_pmetal_receiver_census[2].load()),
        static_cast<unsigned long long>(g_pmetal_receiver_census[3].load()),static_cast<unsigned long long>(g_pmetal_receiver_census[4].load()),static_cast<unsigned long long>(g_pmetal_receiver_census[5].load()),
        static_cast<unsigned long long>(g_pmetal_receiver_census[6].load()),static_cast<unsigned long long>(g_pmetal_receiver_census[7].load()),static_cast<unsigned long long>(g_pmetal_receiver_census[8].load()),
        static_cast<unsigned long long>(g_pmetal_receiver_census[9].load()),static_cast<unsigned long long>(g_pmetal_receiver_census[10].load()),static_cast<unsigned long long>(g_pmetal_receiver_census[11].load()),
        static_cast<unsigned long long>(g_pmetal_receiver_census[12].load()),static_cast<unsigned long long>(g_pmetal_receiver_census[13].load()),static_cast<unsigned long long>(g_pmetal_receiver_census[14].load()),
        static_cast<unsigned long long>(g_pmetal_receiver_census[15].load()),static_cast<unsigned long long>(g_pmetal_receiver_census[16].load()),static_cast<unsigned long long>(g_pmetal_receiver_census[17].load()),
        static_cast<unsigned long long>(g_pmetal_receiver_census[18].load()),static_cast<unsigned long long>(g_pmetal_receiver_census[19].load()),static_cast<unsigned long long>(g_pmetal_receiver_census[20].load()),
        static_cast<unsigned long long>(g_pmetal_receiver_census[21].load()),static_cast<unsigned long long>(g_pmetal_receiver_census[22].load()),static_cast<unsigned long long>(g_pmetal_receiver_census[23].load()),
        static_cast<unsigned long long>(g_pmetal_receiver_other.load()));
    reshade::log::message(reshade::log::level::info,census);
'''.strip()

def main():
    ap=argparse.ArgumentParser();ap.add_argument('--input',type=Path,required=True);ap.add_argument('--output',type=Path,required=True);a=ap.parse_args()
    text=a.input.read_text(encoding='utf-8')
    anchor='std::atomic<std::uint64_t> g_present_reject{0},g_present_nonpmetal{0},g_raw_created{0},g_native_tx{0};'
    if anchor not in text: raise SystemExit('counter anchor missing')
    text=text.replace(anchor,anchor+'\n'+DECL,1)
    old='if(r->kind!=receiver_kind::stock||r->id<33u||r->id>35u){++g_present_reject;++g_fail_open;return std::nullopt;}'
    if old not in text: raise SystemExit('V3.6 PRESENT host gate missing')
    text=text.replace(old,CENSUS,1)
    marker='reshade::log::message(reshade::log::level::info,line);'
    if marker not in text: raise SystemExit('present log marker missing')
    text=text.replace(marker,marker+'\n'+LOG,1)
    text=text.replace('V3.6 Owned Dedicated PS','V3.7 P_Metal Receiver Census')
    text=text.replace('[DSRRL FULL ENVSPEC V3.6]','[DSRRL FULL ENVSPEC V3.7]')
    text=text.replace('V3.6 owned dedicated P_Metal PS island armed','V3.7 exact P_Metal receiver census armed; PRESENT fail-open')
    text=text.replace('Exact build131 + exact P_Metal + exact stock 33/34/35 -> owned DXBC72/73/74 consumer island; raw PTDE RGBA; native PS+t12/t14+s12/s14 transaction; fail-open elsewhere.','Diagnostic exact-P_Metal receiver census across stock receiver IDs 24..47; PRESENT is fail-open; EXPLICIT_NONE remains unchanged.')
    if 'g_pmetal_receiver_census' not in text or 'PRESENT fail-open' not in text: raise SystemExit('census materialization failed')
    a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(text,encoding='utf-8')
    print('PASS V3.7 exact P_Metal receiver census; PRESENT fail-open')
if __name__=='__main__': main()
