#!/usr/bin/env python3
"""Compare two V350 Win64 native addon's RUNTIME_FUNCTION code-shape frontiers.

No PDB required. Matching function *lengths* is not matching instructions:
this audit deliberately does NOT claim equivalent bytecode or call targets.
Usage:
  python renderer-core/tools/audit_v350_qpc_pe_layout.py old.addon64 new.addon64
"""
from __future__ import annotations
import difflib
import hashlib
import json
import struct
import sys
from pathlib import Path

def layout(path: Path) -> dict:
    binary = path.read_bytes()
    pe = struct.unpack_from("<I", binary, 0x3C)[0]
    assert binary[pe:pe+4] == b"PE\\x00\\x00".decode("unicode_escape").encode("latin1"), "PE32+ expected"
    nsec = struct.unpack_from("<H", binary, pe + 6)[0]
    opts = struct.unpack_from("<H", binary, pe + 20)[0]
    sections = {}
    for i in range(nsec):
        offset = pe + 24 + opts + 40 * i
        name = binary[offset:offset+8].split(b"\\x00".decode("unicode_escape").encode("latin1"))[0].decode("ascii")
        vsize, rva, physical, raw = struct.unpack_from("<IIII", binary, offset+8)
        sections[name] = (vsize, rva, raw, physical)
    assert ".pdata" in sections and ".text" in sections
    ps, prva, poff, _ = sections[".pdata"]
    lengths = []
    functions = []
    for off in range(poff, poff + ps - (ps % 12), 12):
        start, end, unwind = struct.unpack_from("<III", binary, off)
        if 0x1000 <= start < end < 0x100000:
            lengths.append(end-start)
            functions.append((start,end))
    return {
        "path": path.name,
        "sha256": hashlib.sha256(binary).hexdigest(),
        "bytes": len(binary),
        "text_virtual_size": sections[".text"][0],
        "exception_functions": len(functions),
        "function_lengths": lengths,
        "function_rvas": functions,
    }

def main() -> None:
    if len(sys.argv) != 3:
        raise SystemExit("usage: audit_v350_qpc_pe_layout.py old.addon64 new.addon64")
    old, new = [layout(Path(p)) for p in sys.argv[1:]]
    ops = difflib.SequenceMatcher(None, old["function_lengths"],
                                   new["function_lengths"], autojunk=False).get_opcodes()
    aligned = sum(i2-i1 for tag, i1, i2, _, _ in ops if tag == "equal")
    changes = []
    for tag, i1, i2, j1, j2 in ops:
        if tag != "equal":
            changes.append({
                "type": tag,
                "old_count": i2-i1,
                "new_count": j2-j1,
                "old_start_rva": hex(old["function_rvas"][i1][0]) if i1<len(old["function_rvas"]) else "",
                "new_start_rva": hex(new["function_rvas"][j1][0]) if j1<len(new["function_rvas"]) else "",
                "old_lengths":old["function_lengths"][i1:i2],
                "new_lengths":new["function_lengths"][j1:j2],
            })
    for obj in (old, new):
        del obj["function_lengths"]
        del obj["function_rvas"]
    print(json.dumps({
        "old":old, "new":new,
        "aligned_equal_length_functions":aligned,
        "changed_length_hunks":changes,
        "delta_text_virtual_bytes":new["text_virtual_size"]-old["text_virtual_size"],
        "interpretation":"Length alignment is a static code-shape signal only; runtime performance and pixel outcome untested.",
    }, indent=2))
if __name__ == "__main__":
    main()
