#!/usr/bin/env python3
from __future__ import annotations
import argparse
from pathlib import Path

REPL={
    'PMetal_EnvSpec_Island_stock33_v40.dxbc':'PMetal_EnvSpec_Island_stock33_v41.dxbc',
    'PMetal_EnvSpec_Island_stock34_v40.dxbc':'PMetal_EnvSpec_Island_stock34_v41.dxbc',
    'PMetal_EnvSpec_Island_stock35_v40.dxbc':'PMetal_EnvSpec_Island_stock35_v41.dxbc',
    '088adca8ed2d6d13e3452a68f1e447af43e23b149679c9bc77ebb8f4b0ac7e35':'6fff10a2d0f660cfa4ba352bc5d8a6211129c245478badcc77dac1ba595b9938',
    'ce63f9722960b324fcc3bf41708b0896129f7099a6b785827e3c0c628f5fb604':'52eb659e7d657c568fc0e0f779706d37aaf9412581f878346ead217d526f9990',
    'a74826ac6f92cd1e38d6e9d45728e6f2901bd1f91204d14bd8c099ab5d6e585d':'5dd6d170f27ea44b428f832680cde99c922fd1bcf7ee5c6971dc26fbfa77f463',
    'V4.0 Operator-Isolated EnvSpec':'V4.1 Preserve-r1.x EnvSpec',
    '[DSRRL FULL ENVSPEC V4.0]':'[DSRRL FULL ENVSPEC V4.1]',
    'V4.0 operator-isolated P_Metal EnvSpec island armed':'V4.1 P_Metal EnvSpec island armed with live r1.x preserved',
}

def main():
    ap=argparse.ArgumentParser(); ap.add_argument('--input',type=Path,required=True); ap.add_argument('--output',type=Path,required=True); a=ap.parse_args()
    text=a.input.read_text(encoding='utf-8')
    for old,new in REPL.items():
        if old not in text: raise SystemExit('missing anchor: '+old)
        text=text.replace(old,new)
    required=('6fff10a2d0f660cfa4ba352bc5d8a6211129c245478badcc77dac1ba595b9938','52eb659e7d657c568fc0e0f779706d37aaf9412581f878346ead217d526f9990','5dd6d170f27ea44b428f832680cde99c922fd1bcf7ee5c6971dc26fbfa77f463','V4.1 Preserve-r1.x EnvSpec')
    for tok in required:
        if tok not in text: raise SystemExit('missing output token: '+tok)
    a.output.parent.mkdir(parents=True,exist_ok=True); a.output.write_text(text,encoding='utf-8')
    print('PASS V4.1 preserve-r1.x sidecar carrier')

if __name__=='__main__': main()
