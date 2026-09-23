from pathlib import Path
import argparse, hashlib, json

PARENT_SHA256 = "4641d2f9d66e879cc86c05aa403a6f9fcfe96c5f9486c15cdae4339f15d50d68"
OUTPUT_SHA256 = "4ddf2b270b09efcc57c4cf680ac0da78ebc3f0a5f58fd7d242b87b90aa6168cc"
EXPECTED_SIZE = 35423232

PATCHES = [
    (1770920, bytes.fromhex('488b9688221100'), bytes.fromhex('e9150100009090'), 'formatter_threadstate_bypass'),
]

def sha256(data):
    return hashlib.sha256(data).hexdigest()

def main():
    ap = argparse.ArgumentParser(description='Byte-exact reconstruction of V15.4 from preserved V15.3')
    ap.add_argument("--base", required=True, help="Exact preserved parent addon")
    ap.add_argument("--out", required=True, help="Output addon path")
    ap.add_argument("--audit", help="Optional JSON audit path")
    args = ap.parse_args()
    data = bytearray(Path(args.base).read_bytes())
    if len(data) != EXPECTED_SIZE or sha256(data) != PARENT_SHA256:
        raise SystemExit("parent identity mismatch")
    applied = []
    for off, old, new, label in PATCHES:
        if len(old) != len(new):
            raise SystemExit(f"length mismatch: {label}")
        got = bytes(data[off:off+len(old)])
        if got != old:
            raise SystemExit(f"patch preimage mismatch {label} @0x{off:X}: {got.hex()} != {old.hex()}")
        data[off:off+len(new)] = new
        applied.append({"offset": hex(off), "old": old.hex(), "new": new.hex(), "label": label})
    result = bytes(data)
    got_sha = sha256(result)
    if got_sha != OUTPUT_SHA256:
        raise SystemExit(f"output identity mismatch: {got_sha} != {OUTPUT_SHA256}")
    Path(args.out).write_bytes(result)
    audit = {"schema":"dsrrl.reconstructed_binary_delta.v1","status":"RECONSTRUCTED_BYTE_EXACT","original_builder_recovered":False,"parent_sha256":PARENT_SHA256,"output_sha256":OUTPUT_SHA256,"size":len(result),"patches":applied}
    if args.audit:
        Path(args.audit).write_text(json.dumps(audit, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(audit, indent=2))

if __name__ == "__main__":
    main()
