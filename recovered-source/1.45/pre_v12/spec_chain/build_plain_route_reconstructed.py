#!/usr/bin/env python3
from pathlib import Path
import hashlib

LABEL='FULL24 -> equipment plain-route expansion'
EXPECTED_SOURCE_SHA='cb95e2332c2d2dfd2184fe8ea04a882ea6f5ff4ea314e421e7eedcf768d5609d'
EXPECTED_TARGET_SHA='ad23449c0f79638a878ed857bcb7220544a7b1526a33e77588336e526658608b'
PATCHES=[
    (0x572e, bytes.fromhex('4280bc2290fd07000090909090'), bytes.fromhex('e8214e100084c00f843c040000')),
    (0x817a0, bytes.fromhex('9a9919'), bytes.fromhex('000000')),
    (0x817a4, bytes.fromhex('9a9919'), bytes.fromhex('000000')),
    (0x817a8, bytes.fromhex('9a9919'), bytes.fromhex('000000')),
    (0x817b2, bytes.fromhex('803f'), bytes.fromhex('2040')),
    (0x817b6, bytes.fromhex('803f'), bytes.fromhex('2040')),
    (0x817ba, bytes.fromhex('803f'), bytes.fromhex('2040')),
    (0x817be, bytes.fromhex('803f'), bytes.fromhex('2040')),
    (0x817c2, bytes.fromhex('803f'), bytes.fromhex('2040')),
    (0x817c6, bytes.fromhex('803f'), bytes.fromhex('2040')),
    (0x817ce, bytes.fromhex('00'), bytes.fromhex('40')),
    (0x817d2, bytes.fromhex('0000'), bytes.fromhex('8040')),
    (0x817ea, bytes.fromhex('e040'), bytes.fromhex('003f')),
    (0x817ee, bytes.fromhex('e040'), bytes.fromhex('003f')),
    (0x817f2, bytes.fromhex('e040'), bytes.fromhex('003f')),
    (0x817fa, bytes.fromhex('0000'), bytes.fromhex('803f')),
    (0x817fe, bytes.fromhex('0000'), bytes.fromhex('803f')),
    (0x81802, bytes.fromhex('0000'), bytes.fromhex('803f')),
    (0x81806, bytes.fromhex('0000'), bytes.fromhex('803f')),
    (0x8180a, bytes.fromhex('0000'), bytes.fromhex('803f')),
    (0x8180e, bytes.fromhex('000000'), bytes.fromhex('803f01')),
    (0x8181a, bytes.fromhex('000000'), bytes.fromhex('e04001')),
    (0x8183c, bytes.fromhex('01'), bytes.fromhex('00')),
    (0x81842, bytes.fromhex('0040'), bytes.fromhex('803f')),
    (0x81846, bytes.fromhex('0040'), bytes.fromhex('803f')),
    (0x8184a, bytes.fromhex('0040a967af'), bytes.fromhex('803f000080')),
    (0x81850, bytes.fromhex('a967af'), bytes.fromhex('000080')),
    (0x81854, bytes.fromhex('a967af'), bytes.fromhex('000080')),
    (0x81858, bytes.fromhex('02'), bytes.fromhex('01')),
    (0x8185e, bytes.fromhex('704103'), bytes.fromhex('000000')),
    (0x81862, bytes.fromhex('0000'), bytes.fromhex('e040')),
    (0x81878, bytes.fromhex('9a9919'), bytes.fromhex('000000')),
    (0x8187c, bytes.fromhex('9a9919'), bytes.fromhex('000000')),
    (0x81880, bytes.fromhex('9a9919'), bytes.fromhex('000000')),
    (0x8188a, bytes.fromhex('0040'), bytes.fromhex('c03f')),
    (0x8188e, bytes.fromhex('0040'), bytes.fromhex('c03f')),
    (0x81892, bytes.fromhex('0040a967af'), bytes.fromhex('c03f0000c0')),
    (0x81898, bytes.fromhex('a967af'), bytes.fromhex('0000c0')),
    (0x8189c, bytes.fromhex('a967af'), bytes.fromhex('0000c0')),
    (0x818a6, bytes.fromhex('704203'), bytes.fromhex('000000')),
    (0x818aa, bytes.fromhex('0000'), bytes.fromhex('e040')),
    (0x818c0, bytes.fromhex('9a9919'), bytes.fromhex('000000')),
    (0x818c4, bytes.fromhex('9a9919'), bytes.fromhex('000000')),
    (0x818c8, bytes.fromhex('9a9919'), bytes.fromhex('000000')),
    (0x818d2, bytes.fromhex('80'), bytes.fromhex('c0')),
    (0x818d6, bytes.fromhex('80'), bytes.fromhex('c0')),
    (0x818da, bytes.fromhex('80'), bytes.fromhex('c0')),
    (0x818de, bytes.fromhex('80'), bytes.fromhex('c0')),
    (0x818e2, bytes.fromhex('80'), bytes.fromhex('c0')),
    (0x818e6, bytes.fromhex('80'), bytes.fromhex('c0')),
    (0x818ee, bytes.fromhex('8040'), bytes.fromhex('0000')),
    (0x818f2, bytes.fromhex('0000'), bytes.fromhex('e040')),
    (0x81908, bytes.fromhex('9a9919'), bytes.fromhex('000000')),
    (0x8190c, bytes.fromhex('9a9919'), bytes.fromhex('000000')),
    (0x81910, bytes.fromhex('9a9919'), bytes.fromhex('000000')),
    (0x81914, bytes.fromhex('02'), bytes.fromhex('00')),
    (0x8191a, bytes.fromhex('0000'), bytes.fromhex('c03f')),
    (0x8191e, bytes.fromhex('0000'), bytes.fromhex('c03f')),
    (0x81922, bytes.fromhex('0000'), bytes.fromhex('c03f')),
    (0x81926, bytes.fromhex('0000'), bytes.fromhex('c03f')),
    (0x8192a, bytes.fromhex('0000'), bytes.fromhex('c03f')),
    (0x8192e, bytes.fromhex('000000'), bytes.fromhex('c03f01')),
    (0x8193a, bytes.fromhex('000000'), bytes.fromhex('e04001')),
    (0x8195c, bytes.fromhex('01'), bytes.fromhex('00')),
    (0x81978, bytes.fromhex('02'), bytes.fromhex('01')),
    (0x8197f, bytes.fromhex('40'), bytes.fromhex('00')),
    (0x81982, bytes.fromhex('0000'), bytes.fromhex('e040')),
    (0x81998, bytes.fromhex('9a9919'), bytes.fromhex('000000')),
    (0x8199c, bytes.fromhex('9a9919'), bytes.fromhex('000000')),
    (0x819a0, bytes.fromhex('9a9919'), bytes.fromhex('000000')),
    (0x819aa, bytes.fromhex('20'), bytes.fromhex('00')),
    (0x819ae, bytes.fromhex('20'), bytes.fromhex('00')),
    (0x819b2, bytes.fromhex('20'), bytes.fromhex('00')),
    (0x819b4, bytes.fromhex('2e21c23f2e21c23f2e21c23f'), bytes.fromhex('000000400000004000000040')),
    (0x819c6, bytes.fromhex('084102'), bytes.fromhex('000000')),
    (0x819ca, bytes.fromhex('0000'), bytes.fromhex('e040')),
    (0x87fa0, bytes.fromhex('65373136356161663835353663666532'), bytes.fromhex('63383530346266613634633834643530')),
    (0x87fb1, bytes.fromhex('39333565373366623162653237616136613433323132386638'), bytes.fromhex('35626335366162363365343364333139656636396166303863')),
    (0x87fcb, bytes.fromhex('633230316162653230'), bytes.fromhex('303434653032363062')),
    (0x87fd5, bytes.fromhex('363239316661393637'), bytes.fromhex('333961623162373866')),
    (0x87fdf, bytes.fromhex('32'), bytes.fromhex('63')),
    (0x87ff0, bytes.fromhex('316133383137393262353139646334306331363961323238653932623662313366656531373163366265343961643062386663'), bytes.fromhex('396432343334376432626661393064663036326633396132383166643061653037663136303230336631646338633564313134')),
    (0x88024, bytes.fromhex('3163306664333539'), bytes.fromhex('6332313861366461')),
    (0x8802d, bytes.fromhex('306631'), bytes.fromhex('346332')),
    (0x88040, bytes.fromhex('66656333366662383238386637363838'), bytes.fromhex('34646566393738323866393561373562')),
    (0x88051, bytes.fromhex('316630396231666132376564'), bytes.fromhex('343634316661396636346261')),
    (0x8805e, bytes.fromhex('3666623562643535386238356363613736'), bytes.fromhex('6663353865393039653639343739633064')),
    (0x88070, bytes.fromhex('61313236316638366661363263373431'), bytes.fromhex('34363061616166396364343636656535')),
    (0x88090, bytes.fromhex('393833'), bytes.fromhex('646565')),
    (0x88094, bytes.fromhex('623839313135303963646533373832386437'), bytes.fromhex('353561386534643530373637616561626139')),
    (0x880a7, bytes.fromhex('33'), bytes.fromhex('62')),
    (0x880a9, bytes.fromhex('61326564323035343231313532313861373463616334333761363230'), bytes.fromhex('39626661393762306662343034663930313234396635636131336661')),
    (0x880c6, bytes.fromhex('38303330626239333239'), bytes.fromhex('36323631306465626461')),
    (0x880e0, bytes.fromhex('63666366663636363139623261643362643837323265'), bytes.fromhex('35333831396561643333376331656364383539336438')),
    (0x880f7, bytes.fromhex('62316632653866333738333737386263343366623736'), bytes.fromhex('33356331666331666464653538396630306562386561')),
    (0x8810e, bytes.fromhex('37633535346636'), bytes.fromhex('31356138616533')),
    (0x88116, bytes.fromhex('61373065356631306163'), bytes.fromhex('37646137313037336561')),
    (0x88130, bytes.fromhex('6537633935'), bytes.fromhex('3066326239')),
    (0x88136, bytes.fromhex('3963376635663530303664303633373831366635643036'), bytes.fromhex('3130313262383363346466633930396333313164323961')),
    (0x8814e, bytes.fromhex('3264376563363935626439303634353438646135343534343763'), bytes.fromhex('3131343764653863313531393162633632333938636530643565')),
    (0x88169, bytes.fromhex('65303131333861'), bytes.fromhex('63633637353765')),
    (0x88180, bytes.fromhex('31343937663533'), bytes.fromhex('36313361333239')),
    (0x88188, bytes.fromhex('326530336163383837363761613537663630323530623864633135643637633635616665373034'), bytes.fromhex('653739303662326465613563306364343862646133376338363238623835306261643939383836')),
    (0x881b0, bytes.fromhex('663066316134'), bytes.fromhex('386134303539')),
    (0x881b7, bytes.fromhex('3938'), bytes.fromhex('6334')),
    (0x881ba, bytes.fromhex('336563'), bytes.fromhex('343965')),
    (0x881be, bytes.fromhex('6433'), bytes.fromhex('6164')),
    (0x881d0, bytes.fromhex('35'), bytes.fromhex('64')),
    (0x881d3, bytes.fromhex('6137383035613835623536366334626265303537'), bytes.fromhex('3835613463303062383434636462316130396235')),
    (0x881e8, bytes.fromhex('3537356261363732623737373130383961353066613932346437313337'), bytes.fromhex('3163623033373334336535626164333132343965376563383065643565')),
    (0x88207, bytes.fromhex('346438376438313231'), bytes.fromhex('613139356239653666')),
    (0x107754, bytes.fromhex('000000000000'), bytes.fromhex('83ff170f8415')),
    (0x10775d, bytes.fromhex('000000000000'), bytes.fromhex('83ff180f840c')),
    (0x107766, bytes.fromhex('000000000000'), bytes.fromhex('83ff190f8403')),
    (0x10776f, bytes.fromhex('0000000000000000000000000000000000000000'), bytes.fromhex('b001c351524883ec38488b4c24404885c90f84a6')),
    (0x107786, bytes.fromhex('0000000000000000000000'), bytes.fromhex('488b01ff104885c00f8498')),
    (0x107794, bytes.fromhex('0000000000000000'), bytes.fromhex('4889442430e86a81')),
    (0x10779e, bytes.fromhex('000000000000'), bytes.fromhex('4885c00f8485')),
    (0x1077a7, bytes.fromhex('00000000000000000000000000000000000000'), bytes.fromhex('48894424204889c1488b5424304531c0e8d882')),
    (0x1077bc, bytes.fromhex('000000000000'), bytes.fromhex('4885c00f8467')),
    (0x1077c5, bytes.fromhex('00000000000000000000'), bytes.fromhex('488b50084885d20f845a')),
    (0x1077d2, bytes.fromhex('0000000000000000'), bytes.fromhex('488b4c2420e8e688')),
    (0x1077dc, bytes.fromhex('000000000000'), bytes.fromhex('4885c00f8447')),
    (0x1077e5, bytes.fromhex('00000000'), bytes.fromhex('48837808')),
    (0x1077ea, bytes.fromhex('000000'), bytes.fromhex('0f8535')),
    (0x1077f0, bytes.fromhex('000000'), bytes.fromhex('837814')),
    (0x1077f4, bytes.fromhex('000000'), bytes.fromhex('0f8532')),
    (0x1077fa, bytes.fromhex('00000000000000000000000000'), bytes.fromhex('4889442428488b4c2430e80383')),
    (0x107809, bytes.fromhex('0000000000000000'), bytes.fromhex('488b4c2430e83289')),
    (0x107813, bytes.fromhex('000000000000000000'), bytes.fromhex('488b44242848837808')),
    (0x10781d, bytes.fromhex('0000000000'), bytes.fromhex('0f95c0e909')),
    (0x107825, bytes.fromhex('00000000'), bytes.fromhex('b001e902')),
    (0x10782c, bytes.fromhex('000000000000000000'), bytes.fromhex('31c04883c4385a59c3')),
]

def sha(b): return hashlib.sha256(b).hexdigest()

def rebuild(source: bytes) -> bytes:
    if sha(source) != EXPECTED_SOURCE_SHA:
        raise ValueError(f'source SHA mismatch: {sha(source)} != {EXPECTED_SOURCE_SHA}')
    b=bytearray(source)
    for off,old,new in PATCHES:
        got=bytes(b[off:off+len(old)])
        if got != old:
            raise ValueError(f'guard mismatch at {off:#x}: {got.hex()} != {old.hex()}')
        b[off:off+len(new)] = new
    out=bytes(b)
    if sha(out) != EXPECTED_TARGET_SHA:
        raise ValueError(f'target SHA mismatch: {sha(out)} != {EXPECTED_TARGET_SHA}')
    return out

if __name__ == '__main__':
    import argparse
    ap=argparse.ArgumentParser()
    ap.add_argument('source')
    ap.add_argument('output')
    ns=ap.parse_args()
    out=rebuild(Path(ns.source).read_bytes())
    Path(ns.output).write_bytes(out)
    print(f'{LABEL}: EXACT PASS {sha(out)} patches={len(PATCHES)}')
