#!/usr/bin/env python3
"""Read-only Source1 RX34 asset provenance audit. No render-route changes.

The generic equipment name wp_a_0200_s is NOT silently treated as the
character-specific c2500_WP_A_0200_s resource. PTDE DDS content must be
certified independently before any PTDE companion can be authorized.
"""
import argparse
import hashlib
import json
from pathlib import Path

STOCK = (
    ("Diffuse","c2500_WP_A_0200","07897dbf4ed2fd72"),
    ("Specular","c2500_WP_A_0200_s","236adf2eb0a2afcc"),
    ("Normals","c2500_WP_A_0200_n","236af42eb0a2d37b"),
)

def fnv_identity(name):
    h=14695981039346656037
    for b in name.lower().encode("ascii"):
        h=((h ^ b)*1099511628211)&((1<<64)-1)
    return f"{h:016x}"

def audit(game_dir):
    records=[]
    for asset_class,name,expected in STOCK:
        if fnv_identity(name)!=expected:
            raise ValueError("Source1 certified stock identity drift")
        file=Path(game_dir)/"DSRRL"/asset_class/(name+".dds")
        item={"asset_class":asset_class,"dsr_name":name,"dsr_hash":expected,
              "exact_expected_path":str(file),"dds_exists":file.is_file(),
              "ptde_origin_certified":False,"allowed_to_replace":False}
        if file.is_file():
            data=file.read_bytes()
            item.update(bytes=len(data),
                        file_sha256=hashlib.sha256(data).hexdigest(),
                        dds_magic_present=len(data)>=128 and data[:4]==b"DDS ")
        records.append(item)
    return {"scope":"Source1 RX34 Quelaag P_Metal slot2 route345",
            "state":"FAIL_OPEN_UNTIL_EXACT_PTDE_ASSET_CERTIFIED",
            "resources":records,"never_infer_alias_from":["wp_a_0200_s","c2500_WP_A_0200_s"]}

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument("--game-dir",type=Path,required=True)
    parser.add_argument("--json-out",type=Path)
    args=parser.parse_args()
    report=json.dumps(audit(args.game_dir),indent=2)+"\n"
    if args.json_out:
        args.json_out.write_text(report,encoding="utf-8")
    print(report)

if __name__=="__main__":
    main()
