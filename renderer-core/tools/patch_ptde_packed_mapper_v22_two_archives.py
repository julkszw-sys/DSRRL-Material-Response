#!/usr/bin/env python3
"""Reproducible *strict* source patch from packaged v2.1 to v2.2.
Apply in unpacked kit folder only; do NOT modify PTDE/DSR on disk.
ZIP v2.2 also contains patched files already; this is history/reproduction.
"""
from pathlib import Path
import sys
if len(sys.argv)!=2:
    raise SystemExit("Usage: python patch_ptde_packed_mapper_v22_two_archives.py path/to/DSRRL_PTDE_PACKED_ARMOR_MAPPER_V2")
root=Path(sys.argv[1]).resolve()
cs=root/"src/ArmorIndex/PackedCatalog.cs"
src=cs.read_text("utf-8")
old='''        if(headings.Count<4)
            throw new InvalidDataException("Expected dvdbnd0-3.bhd5 alongside corresponding BDT. Found: "+headings.Count);'''
new='''        var numbers = headings.Select(path => {
            var name = Path.GetFileNameWithoutExtension(path).ToLowerInvariant();
            return name.StartsWith("dvdbnd") && int.TryParse(name.Substring(6), out int n) ? n : -1;
        }).OrderBy(n => n).ToArray();
        if (numbers.Length < 2 || numbers[0] != 0 || numbers[1] != 1 ||
            numbers.Where((n,i) => n != i || n>3 || n<0).Any())
            throw new InvalidDataException("Missing mandatory dvdbnd0/1 or noncontiguous PTDE BHD5 archives; count=" + headings.Count);'''
if src.count(old)!=1:
    raise SystemExit("Refusing source drift: expected exactly one v2.1 4-pair assertion")
cs.write_text(src.replace(old,new),"utf-8")
print("PTDE_V22_CSHARP_ARCHIVE_2TO4_LAYOUT_PATCH_PASS (source only; content completeness still OPEN)")
