#!/usr/bin/env python3
"""Read-only audit of PTDE DDS sidecar inventory vs exact SpecRGB V12 names.
Does NOT authorize FLVER slots, shader consumers, or PTDE pixels.
Does not move, modify, copy or upload game assets.
"""
import argparse, csv, json
from pathlib import Path
from collections import defaultdict, Counter

EQUIPMENT_PREFIXES = ("bd_", "lg_", "am_", "hd_", "wp_")
V12_ALL, V12_EQUIPMENT = 769, 748

def corpus(path):
    with path.open(encoding="utf-8-sig", newline="") as f:
        rows=list(csv.DictReader(
            (s for s in f if s.strip() and not s.startswith("#")),delimiter="\t"))
    names={r["logical_name"].casefold() for r in rows}
    equipment={x for x in names if x.startswith(EQUIPMENT_PREFIXES)}
    if len(rows)!=V12_ALL or len(names)!=V12_ALL or len(equipment)!=V12_EQUIPMENT:
        raise ValueError("Wrong canonical SpecRGB V12 corpus")
    return names,equipment

def inventory(folder, names, equipment):
    if not folder.is_dir():
        raise FileNotFoundError(str(folder))
    entries=sorted((x for x in folder.rglob("*") if x.is_file()),
                   key=lambda x:x.relative_to(folder).as_posix().casefold())
    ext=Counter()
    recognized=defaultdict(list)
    invalid=[]
    nested=[]
    for file in entries:
        rel=file.relative_to(folder).as_posix()
        ext[file.suffix.casefold() or "<none>"]+=1
        if file.suffix.casefold()!=".dds": continue
        if file.parent!=folder:
            nested.append(rel) # v203 sidecar_path reads direct children only
            continue
        try:
            with file.open("rb") as f: valid=f.read(4)==b"DDS "
        except OSError:
            valid=False
        if not valid:
            invalid.append(rel)
            continue
        recognized[file.stem.casefold()].append(rel)
    duplicate={name:loc for name,loc in recognized.items() if len(loc)>1}
    usable=set(recognized)-set(duplicate)
    return {
       "schema":"dsrrl.v203.specular_sidecar_inventory.v1",
       "authority":"INVENTORY_ONLY_NOT_FLVER_OR_CONSUMER_OR_PIXELS",
       "folder":str(folder.resolve()),
       "total_files":len(entries),
       "extension_counts":dict(ext),
       "unique_root_dds_usable":len(usable),
       "canonical_v12_equipment_names":len(equipment),
       "equipment_present":len(usable&equipment),
       "equipment_missing":len(equipment-usable),
       "canonical_other_present":len(usable&(names-equipment)),
       "unmatched_root_dds_count":len(usable-names),
       "unmatched_root_dds_sample":sorted(usable-names)[:30],
       "missing_equipment_names":sorted(equipment-usable),
       "invalid_dds":invalid,
       "nested_dds_inaccessible_from_v203_sidecar_path":nested,
       "casefold_duplicate_names":duplicate,
       "pixel":"OPEN"
    }

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--game-root",type=Path)
    ap.add_argument("--sidecar-dir",type=Path)
    ap.add_argument("--corpus",type=Path,
                    default=Path(__file__).resolve().parents[1]/
                    "data/provenance/spec_routes_v12.tsv")
    ap.add_argument("--out",type=Path)
    args=ap.parse_args()
    if not args.game_root and not args.sidecar_dir:
        ap.error("Specify --game-root or --sidecar-dir")
    names,equipment=corpus(args.corpus)
    folder=args.sidecar_dir or args.game_root/"DSRRL"/"Specular"
    result=inventory(folder,names,equipment)
    if args.game_root:
        result["candidate_directories"]={
            str(p):p.is_dir() for p in (
                args.game_root/"DSRRL"/"Specular",
                args.game_root/"DSRRL"/"Speculars",
                args.game_root/"DSRRL_PTDE_SPEC")
        }
    output=json.dumps(result,indent=2,ensure_ascii=False)+"\n"
    if args.out:args.out.write_text(output,encoding="utf-8")
    print(output)

if __name__=="__main__":main()
