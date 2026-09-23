#!/usr/bin/env python3
from pathlib import Path
import argparse, hashlib, json
ROOT=Path(__file__).resolve().parent
INPUT_SHA="a5dc6f817e75e157bd92ee945830d0dd3028ba887f544ebf6cff9976e149c900"
OUTPUT_SHA="6df899936098ac3a22751224958090c5860294e80769e8cc6afd8e0ae316da0a"
WINDOWS=[{'offset': 1582466, 'length': 1264, 'basis_window_sha256': '13aae15282dc969c69b6dc2f25f1b029422a09f983b42cf35b3553b1f1d39b83', 'target_window_sha256': '8b01237a1934da3272804384d967cf9e0e992ddc82bd724277ef348b890cc1c1', 'file': 'safe_rc2_payloads/window0_target.hex'}, {'offset': 1590276, 'length': 61045, 'basis_window_sha256': '5cc1d6e0b043d1dbc24723373b783d60179e08553bb7c19c5cdb74b37bf983ed', 'target_window_sha256': 'cae980a7fc4f5ccbd62f5949361972e6c85aae5496fa5ef07f340373685adab9', 'file': 'safe_rc2_payloads/window1_target.hex'}, {'offset': 1654742, 'length': 17, 'basis_window_sha256': 'c8821e19f8b20c13f75156dbc0192c04aebe7736de254d32174be89b886fe6ea', 'target_window_sha256': '1ef4e85460100a4b8575f7eecd85caf42e09a1a91f31ae3aa4d16813757f2182', 'file': 'safe_rc2_payloads/window2_target.hex'}, {'offset': 1792768, 'length': 68, 'basis_window_sha256': '1751ac12e70e15b4f76c16775cd329ae55973b612521dab2de828a5cdb6c8ab3', 'target_window_sha256': 'bdbcd208e37cd28f72d8f8ffa1b1ab829a74b078fd007bf927a62b0d73dcf048', 'file': 'safe_rc2_payloads/window3_target.hex'}]

def sha(b): return hashlib.sha256(b).hexdigest()
def main():
    ap=argparse.ArgumentParser(description="Byte-exact reconstructed Material Response 1.45 P_Metal R11F Safe Operator Island RC2 materializer")
    ap.add_argument("basis",type=Path); ap.add_argument("output",type=Path); a=ap.parse_args()
    data=bytearray(a.basis.read_bytes())
    if sha(data)!=INPUT_SHA: raise SystemExit(f"refusing unknown basis {sha(data)}")
    for w in WINDOWS:
        off=w["offset"]; n=w["length"]
        old=bytes(data[off:off+n])
        if sha(old)!=w["basis_window_sha256"]: raise SystemExit(f"window guard mismatch {off:#x}")
        new=bytes.fromhex((ROOT/w["file"]).read_text().strip())
        if len(new)!=n or sha(new)!=w["target_window_sha256"]: raise SystemExit(f"payload mismatch {off:#x}")
        data[off:off+n]=new
    out=bytes(data)
    if sha(out)!=OUTPUT_SHA: raise SystemExit(f"output mismatch {sha(out)}")
    a.output.parent.mkdir(parents=True,exist_ok=True); a.output.write_bytes(out)
    print("PASS",OUTPUT_SHA)
if __name__=="__main__": main()
