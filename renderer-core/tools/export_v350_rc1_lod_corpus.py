#!/usr/bin/env python3
"""Validate and export original 1295-name October SpecRGB inventory."""
from pathlib import Path
import argparse,base64,hashlib,re,zlib
ROOT=Path(__file__).resolve().parents[2]
INPUT=ROOT/'renderer-core/data/provenance/sidecar_specular_20261010_names.b85'
SHA='6f417110f6085ca8389213578eb9c0a86c100070717a9df20d62790052c151e4'
DIRECT={'am_f_9450_l_s','am_m_9372_l_s','bd_m_9440_1_l_s','bd_m_9440_2_l_s','wp_a_0652_l_s'}
UNKNOWN={'wp_a_0911_s','wp_a_1004_s'}
def main():
 p=argparse.ArgumentParser();p.add_argument('--output',type=Path);a=p.parse_args()
 raw=zlib.decompress(base64.b85decode(''.join(INPUT.read_text('ascii').split())))
 assert hashlib.sha256(raw).hexdigest()==SHA,'CORPUS_SHA_DRIFT'
 names=raw.decode('ascii').splitlines()
 assert len(names)==1295 and names==sorted(set(names)),'CORPUS_ORDER_DRIFT'
 assert all(re.fullmatch(r'(?:am|bd|lg|hd|wp)_[a-z0-9_]+_s',n) for n in names)
 low={n for n in names if n.endswith('_l_s')}
 assert len(low)==549 and DIRECT<=low and len(low-DIRECT)==544
 assert UNKNOWN<=set(names) and not (UNKNOWN&low)
 if a.output:
  a.output.parent.mkdir(parents=True,exist_ok=True)
  a.output.write_bytes(raw)
 print('SAFE_V350_LOWLOD_CORPUS_PASS names=1295 low=549 candidates=544 direct_protected=5 unresolved=2 sha='+SHA)
if __name__=='__main__':main()
