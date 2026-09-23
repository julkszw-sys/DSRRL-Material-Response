from pathlib import Path
import argparse, hashlib, json

PARENT_SHA256 = "bf552c8606e27ab1bf3535b27509e91d8692fe553993a0e80b767cd30c1dd283"
OUTPUT_SHA256 = "9f112214a7cfc058775df26c58d4e8afaaa40fa22015613520bcaafcc9d5d21b"
EXPECTED_SIZE = 35423232

PATCHES = [
    (1770920, bytes.fromhex('e9150100009090'), bytes.fromhex('488b9688221100'), 'reenable_formatter_threadstate'),
    (1786530, bytes.fromhex('0823'), bytes.fromhex('6822'), 'relocate_uninit_state_ref_00'),
    (1786541, bytes.fromhex('0923'), bytes.fromhex('6922'), 'relocate_uninit_state_ref_01'),
    (1786553, bytes.fromhex('0a23'), bytes.fromhex('6a22'), 'relocate_uninit_state_ref_02'),
    (1786565, bytes.fromhex('0b23'), bytes.fromhex('6b22'), 'relocate_uninit_state_ref_03'),
    (1786577, bytes.fromhex('0c23'), bytes.fromhex('6c22'), 'relocate_uninit_state_ref_04'),
    (1786589, bytes.fromhex('0d23'), bytes.fromhex('6d22'), 'relocate_uninit_state_ref_05'),
    (1786601, bytes.fromhex('0e23'), bytes.fromhex('6e22'), 'relocate_uninit_state_ref_06'),
    (1786613, bytes.fromhex('0f23'), bytes.fromhex('6f22'), 'relocate_uninit_state_ref_07'),
    (1786625, bytes.fromhex('1023'), bytes.fromhex('7022'), 'relocate_uninit_state_ref_08'),
    (1786637, bytes.fromhex('1123'), bytes.fromhex('7122'), 'relocate_uninit_state_ref_09'),
    (1786649, bytes.fromhex('1223'), bytes.fromhex('7222'), 'relocate_uninit_state_ref_10'),
    (1786661, bytes.fromhex('1323'), bytes.fromhex('7322'), 'relocate_uninit_state_ref_11'),
    (1786673, bytes.fromhex('1423'), bytes.fromhex('7422'), 'relocate_uninit_state_ref_12'),
    (1786685, bytes.fromhex('1523'), bytes.fromhex('7522'), 'relocate_uninit_state_ref_13'),
    (1786697, bytes.fromhex('1623'), bytes.fromhex('7622'), 'relocate_uninit_state_ref_14'),
]

def sha256(data):
    return hashlib.sha256(data).hexdigest()

def main():
    ap = argparse.ArgumentParser(description='Byte-exact reconstruction of V15.6 thread-state re-enable from preserved V15.5')
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
