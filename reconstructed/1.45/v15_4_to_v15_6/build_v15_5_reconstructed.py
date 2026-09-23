from pathlib import Path
import argparse, hashlib, json

PARENT_SHA256 = "4ddf2b270b09efcc57c4cf680ac0da78ebc3f0a5f58fd7d242b87b90aa6168cc"
OUTPUT_SHA256 = "bf552c8606e27ab1bf3535b27509e91d8692fe553993a0e80b767cd30c1dd283"
EXPECTED_SIZE = 35423232

PATCHES = [
    (1776886, bytes.fromhex('0023'), bytes.fromhex('6022'), 'relocate_state_ref_00'),
    (1777040, bytes.fromhex('0423'), bytes.fromhex('6422'), 'relocate_state_ref_01'),
    (1777068, bytes.fromhex('0423'), bytes.fromhex('6422'), 'relocate_state_ref_02'),
    (1777218, bytes.fromhex('0023'), bytes.fromhex('6022'), 'relocate_state_ref_03'),
    (1778627, bytes.fromhex('0823'), bytes.fromhex('6822'), 'relocate_state_ref_04'),
    (1778644, bytes.fromhex('0923'), bytes.fromhex('6922'), 'relocate_state_ref_05'),
    (1778662, bytes.fromhex('0a23'), bytes.fromhex('6a22'), 'relocate_state_ref_06'),
    (1778680, bytes.fromhex('0b23'), bytes.fromhex('6b22'), 'relocate_state_ref_07'),
    (1778698, bytes.fromhex('0c23'), bytes.fromhex('6c22'), 'relocate_state_ref_08'),
    (1778716, bytes.fromhex('0d23'), bytes.fromhex('6d22'), 'relocate_state_ref_09'),
    (1778734, bytes.fromhex('0e23'), bytes.fromhex('6e22'), 'relocate_state_ref_10'),
    (1778752, bytes.fromhex('0f23'), bytes.fromhex('6f22'), 'relocate_state_ref_11'),
    (1778770, bytes.fromhex('1023'), bytes.fromhex('7022'), 'relocate_state_ref_12'),
    (1778788, bytes.fromhex('1123'), bytes.fromhex('7122'), 'relocate_state_ref_13'),
    (1778806, bytes.fromhex('1223'), bytes.fromhex('7222'), 'relocate_state_ref_14'),
    (1778824, bytes.fromhex('1323'), bytes.fromhex('7322'), 'relocate_state_ref_15'),
    (1778842, bytes.fromhex('1423'), bytes.fromhex('7422'), 'relocate_state_ref_16'),
    (1778860, bytes.fromhex('1523'), bytes.fromhex('7522'), 'relocate_state_ref_17'),
    (1778878, bytes.fromhex('1623'), bytes.fromhex('7622'), 'relocate_state_ref_18'),
]

def sha256(data):
    return hashlib.sha256(data).hexdigest()

def main():
    ap = argparse.ArgumentParser(description='Byte-exact reconstruction of V15.5 BSS relocation from preserved V15.4')
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
