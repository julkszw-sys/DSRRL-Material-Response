#!/usr/bin/env python3
"""V257 sidecar discovery acceptance and rejection contract."""
from pathlib import Path
from tempfile import TemporaryDirectory
import hashlib
from inventory_dsrll_sidecars import scan

with TemporaryDirectory() as tmp:
    root=Path(tmp)/"DSRRL"
    (root/"Specular").mkdir(parents=True)
    (root/"EnvSpec"/"PackedGI").mkdir(parents=True)
    spec=b"DDS "+bytes(range(36))
    (root/"Specular"/"verified_file.dds").write_bytes(spec)
    pack=b"PTDE fake packed fixture - not an authentic donor"
    (root/"EnvSpec"/"PackedGI"/"PTDE_GI_ENVSPEC_PACK_RGBA.bin").write_bytes(pack)
    files=scan(root)
    assert len(files)==2
    assert all(x["mapping_status"]=="UNVERIFIED_CANDIDATE" for x in files)
    assert all(x["authorizes_runtime_bridge"] is False for x in files)
    assert any(x["sha256"]==hashlib.sha256(spec).hexdigest() for x in files)
    assert any(x["sha256"]==hashlib.sha256(pack).hexdigest() for x in files)
    (root/"Specular"/"bad.dds").write_bytes(b"BAD")
    try:
        scan(root)
        raise AssertionError("Malformed DDS admitted")
    except ValueError:
        pass
print("V257_PHYSICAL_SIDECAR_CANDIDATE_INVENTORY_PASS")
