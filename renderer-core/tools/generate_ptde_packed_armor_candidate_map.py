#!/usr/bin/env python3
"""Reproduce exact candidate PTDE armor BHD5 paths (not texture payloads).

Source: Nordgaren/UXM-Selective-Unpack @ 9501be87e272b6dae55e60a13c3f4753ca6fb3bb
UXM/res/DarkSoulsDictionary.txt. DS1 path hash: slash+lowercase, uint32 *37.
The archive candidates are only the first operator/resource mapping cut.
"""
import argparse
import csv
import pathlib
import re

ARMOR = re.compile(r'^/parts/(am|bd|hd|lg|fa|fc)_.*\.(?:partsbnd|flver)(?:\.dcx)?$', re.I)
def hash37(path):
    path=path.strip().replace('\\','/').lower()
    if not path.startswith('/'):path='/'+path
    h=0
    for c in path:h=(h*37+ord(c))&0xffffffff
    return h

def main():
    a=argparse.ArgumentParser(description=__doc__)
    a.add_argument('--dictionary',type=pathlib.Path,required=True)
    a.add_argument('--output',type=pathlib.Path,required=True)
    args=a.parse_args()
    lines=args.dictionary.read_text('utf-8-sig').splitlines()
    selected=sorted({s.strip() for s in lines if ARM.fullmatch(s.strip())},key=str.casefold)
    if len(selected)!=1437:
        raise ValueError(f'Pinned PTDE candidate count drift: {len(selected)} != 1437')
    rows=[]
    for path in selected:
        low=path.lower().lstrip('/')
        part=ARM.fullmatch(path).group(1).lower()
        variant='L' if '_l.partsbnd' in low else ('M' if '_m.partsbnd' in low else 'BASE')
        rows.append([low,f'{hash37(path):08x}',part,variant,'PTDE_DICTIONARY_CANDIDATE_NOT_PAYLOAD_VALIDATED'])
    if len(set(row[1] for row in rows))!=len(rows):
        raise ValueError('BHD5 hash collision in archive candidates')
    args.output.parent.mkdir(parents=True,exist_ok=True)
    with args.output.open('w',newline='',encoding='utf-8') as file:
        writer=csv.writer(file,lineterminator='\n')
        writer.writerow(['virtual_path','bhd5_path_hash_hex','part_class','quality_variant','authority'])
        writer.writerows(rows)
    print(f'PTDE_ARMOR_ARCHIVE_CANDIDATES_PASS count={len(rows)} L_or_M={sum(r[3]!="BASE" for r in rows)}')
    print('NOT_TPF_CONTENT_COVERAGE; NO_DSR_FLVER_PIXEL_CLAIMS')

if __name__=='__main__':
    main()
