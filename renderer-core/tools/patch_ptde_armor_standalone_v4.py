#!/usr/bin/env python3
"""Replay V4 FLVER0 mesh-header correction against V3 standard-library scanner.

V3 source SHA256 954a0b572060acfb23c3fba49228a36eb08de53e2fa03b4770f5ae70658b73f4
V4 result SHA256 5ff47497212d16831d595d622d047a28cc9bd4d8a1c99bc9093c5482f122bca8
Pure Python; does not open any game files. No external dependency.
"""
from pathlib import Path
import hashlib
import sys
assert len(sys.argv)==3,"usage: patch_flver0_standalone_v4.py V3.py V4.py"
original=Path(sys.argv[1]).read_bytes()
assert hashlib.sha256(original).hexdigest()=="954a0b572060acfb23c3fba49228a36eb08de53e2fa03b4770f5ae70658b73f4","wrong V3 source"
s=original.decode("utf-8")
def swap(before,after):
 global s
 n=s.count(before)
 if n!=1:raise ValueError("anchor drift "+repr(before[:70])+f" ({n} matches)")
 s=s.replace(before,after,1)
swap("""    # FLVER0 layout from SoulsFormatsNEXT: header 0x80, dummy 0x40,
    # material 0x20, node 0x80, mesh 0x60.
    material_base=0x80+dummy_count*0x40
    mesh_base=material_base+mat_count*0x20+node_count*0x80
    check(data,material_base,mat_count*0x20,'FLVER0 material headers')
    check(data,mesh_base,mesh_count*0x60,'FLVER0 mesh headers')
    used={u8(data,mesh_base+i*0x60+1) for i in range(mesh_count)}""",
"""    # Actual FLVER0 mesh header is 0x64 (100 bytes), NOT 0x60.
    # SoulsFormats FLVER0.Mesh reads: 4 bytes flags/material/cull/strip,
    # 2x I32 counts, I16 node, 28x I16 bones, I16 used bones,
    # 7x I32 remainder = 100 bytes total. A 0x60 stride corrupts the
    # material-index census on multi-mesh armor (the prior universal fail).
    MESH_STRIDE = 0x64
    material_base=0x80+dummy_count*0x40
    mesh_base=material_base+mat_count*0x20+node_count*0x80
    check(data,material_base,mat_count*0x20,'FLVER0 material headers')
    check(data,mesh_base,mesh_count*MESH_STRIDE,'FLVER0 mesh headers')
    used={u8(data,mesh_base+i*MESH_STRIDE+1) for i in range(mesh_count)}""")
swap("    # Join reference to PTDE material source. Never alias LOD names or rename DSR resource.\n",
"""    # Show the actual decoding failure directly in console; a silent wall of
    # 1437 parse errors is unacceptable. Preserve full details in CSV.
    if errors:
        err_kind=Counter((r['stage'],r['error']) for r in errors)
        print('[FAILURES] Most frequent decoder errors (stage, message, count):',flush=True)
        for (stage,msg),n in err_kind.most_common(8):
            print(f'  {n}x {stage}: {msg}',flush=True)
        for example in errors[:3]:
            print('[FAILURE_EXAMPLE]',example,flush=True)
    # Join reference to PTDE material source. Never alias LOD names or rename DSR resource.
""")
swap("    (out/'SUMMARY.json').write_text(json.dumps(overview,ensure_ascii=False,indent=2),encoding='utf-8')",
"""    overview['decoder_errors_by_stage']=dict(Counter(e['stage'] for e in errors))
    overview['scan_valid']=bool(models and references and textures and not any(e['stage'] in ('BHD_DCX','BND3','FLVER','TPF') for e in errors))
    (out/'SUMMARY.json').write_text(json.dumps(overview,ensure_ascii=False,indent=2),encoding='utf-8')""")
swap("    return 0 if models and references and textures else 2",
     "    return 0 if overview['scan_valid'] else 2")
out=s.encode("utf-8")
assert hashlib.sha256(out).hexdigest()=="5ff47497212d16831d595d622d047a28cc9bd4d8a1c99bc9093c5482f122bca8","replay digest mismatch"
Path(sys.argv[2]).write_bytes(out)
print("PTDE_ARMOR_V4_EXACT_REPLAY_PASS, FLVER0_MESH=0x64, fail-closed")
