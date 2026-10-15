#!/usr/bin/env python3
"""V282: compile STATIC texture candidates and independent MTD donors.

Read-only/offline. Neither texture name/hash nor MTD route alone authorizes an
atomic bridge. This planner does not change the DSR renderer or create shaders.

Usage:
  python renderer-core/tools/v282/compile_prebind_candidates.py --root . --output /tmp/v282.json
  python ... --root . --inventory /path/to/normalized_20261010_inventory.json --output /tmp/v282.json

Optional inventory format: {"entries":[{"path":"Specular/FOO_s.dds","sha256":"<64 hex>"},...]}.
This input reports existence/provenance only; original DDS bytes and D3D11 SRV
identity are still required for runtime authorization.
"""
from __future__ import annotations
import argparse
import collections
import csv
import json
import re
from pathlib import Path

HEX64 = re.compile(r"^[0-9a-fA-F]{64}$")
HEX16 = re.compile(r"^[0-9a-fA-F]{16}$")
CONST = re.compile(r"\{(\d+)u,\{\{([^}]+)\}\},([0-9.]+)f,(true|false)\}")


def read_tsv(path: Path):
    with path.open(encoding="utf-8", newline="") as f:
        rows = [line for line in f if line.strip() and not line.startswith("#")]
    return list(csv.DictReader(rows, delimiter="\t"))


def manifest_path(record):
    raw = str(record.get("path") or record.get("relative_path") or "")
    pieces = raw.replace("\\", "/").split("/")
    if len(pieces) < 2 or pieces[-2].casefold() not in (
        "specular", "diffuse", "normals"
    ) or not pieces[-1].casefold().endswith(".dds"):
        raise ValueError("unexpected inventory relative DDS path: " + raw)
    return pieces[-2].casefold(), pieces[-1][:-4].casefold()


def inventory_index(path: Path | None):
    if path is None:
        return None
    data = json.loads(path.read_text(encoding="utf-8"))
    entries = data["entries"] if isinstance(data, dict) else data
    if not isinstance(entries, list):
        raise ValueError("inventory must have a list of entries")
    result = {}
    for row in entries:
        cls, name = manifest_path(row)
        sha = str(row.get("sha256") or "").lower()
        if not HEX64.fullmatch(sha):
            raise ValueError("inventory entry missing exact DDS sha256: " + name)
        key = (cls, name)
        if key in result and result[key] != sha:
            raise ValueError("ambiguous inventory class/name: " + str(key))
        result[key] = sha
    return result


def constants_from_header(path: Path):
    source = path.read_text(encoding="utf-8")
    result = {}
    for match in CONST.finditer(source):
        index = int(match.group(1))
        values = [float(x.rstrip("f")) for x in match.group(2).split(",")]
        if len(values) != 3 or index in result:
            raise ValueError("ambiguous material c100 route")
        result[index] = dict(c100=values, ptde_specular_power=float(match.group(3)),
                             power_verified=match.group(4) == "true")
    if not result:
        raise ValueError("no verified material constants parsed")
    return result


def compile_index(root: Path, inventory: Path | None):
    p = root / "renderer-core/data/provenance"
    spec = read_tsv(p / "spec_routes_v12.tsv")
    diffuse = read_tsv(p / "diffuse_routes_v12.tsv")
    normal = read_tsv(p / "normal_routes_v12.tsv")
    inv = inventory_index(inventory)
    by_spec = {}
    for r in spec:
        key = r["spec_hash"].lower()
        name = r["logical_name"].casefold()
        if not HEX16.fullmatch(key) or key in by_spec:
            raise ValueError("bad / repeated spec logical hash: " + key)
        by_spec[key] = dict(logical_name=name, diffuse=[], normal=[])

    diffuse_without_spec = []
    for row in diffuse:
        key, dh = row["spec_hash"].lower(), row["diffuse_hash"].lower()
        if not HEX16.fullmatch(key) or not HEX16.fullmatch(dh):
            raise ValueError("malformed diffuse pair")
        record = dict(diffuse_hash=dh, logical_names=row["logical_names"],
                      row_uses=int(row["row_uses"]))
        if key not in by_spec:
            # Legitimate pair-source row but NO matching approved name lookup.
            # Preserve the evidence and block its early identity admission.
            diffuse_without_spec.append(dict(spec_hash=key, **record))
            continue
        by_spec[key]["diffuse"].append(record)
    normal_without_spec = []
    for row in normal:
        key = row["spec_hash"].lower()
        r = dict(diffuse_hash=row["diffuse_hash"].lower(),
                 normal_hash=row["normal_hash"].lower(),
                 logical_names=row["logical_names"],
                 row_uses=int(row["row_uses"]))
        if not all(HEX16.fullmatch(r[field]) for field in ("diffuse_hash", "normal_hash")):
            raise ValueError("malformed normal triple")
        if key not in by_spec:
            normal_without_spec.append(dict(spec_hash=key, **r))
        else:
            by_spec[key]["normal"].append(r)

    constants = constants_from_header(root /
        "renderer-core/include/dsrrl/operators/material_response/generated_material_constants_v1.hpp")
    routes = json.loads((root / "renderer-core/data/material_response_routes_v1.json").
                        read_text(encoding="utf-8"))["routes"]
    materials = []
    for row in routes:
        idx = row["route_index"]
        if not HEX64.fullmatch(row["sha256"]) or idx not in constants:
            raise ValueError("MTD donor without valid exact hash/constants")
        materials.append(dict(mtd_name=row["mtd_name"], mtd_sha256=row["sha256"],
                              route_index=idx, family=row["material_family"],
                              receiver_ids=row["receiver_triplet"], c101=row["c101"],
                              lod_min=row["lod_min"], lod_max=row["lod_max"],
                              **constants[idx]))

    if inv is not None:
        for entry in by_spec.values():
            sha = inv.get(("specular", entry["logical_name"]))
            entry["sidecar_specular_sha256"] = sha
            entry["inventory_present"] = sha is not None

    anomalies = [dict(spec_hash=key, logical_name=entry["logical_name"],
                      diffuse_candidates=len(entry["diffuse"]),
                      normal_candidates=len(entry["normal"]))
                 for key, entry in by_spec.items()
                 if len(entry["diffuse"]) > 1 or len(entry["normal"]) > 1]

    unresolved_groups = collections.defaultdict(lambda: [0, 0])
    for entry in diffuse_without_spec:
        unresolved_groups[entry["spec_hash"]][0] += 1
    for entry in normal_without_spec:
        unresolved_groups[entry["spec_hash"]][1] += 1
    ambiguous_missing = [
        dict(spec_hash=k, diffuse_candidates=v[0], normal_candidates=v[1])
        for k, v in unresolved_groups.items() if v[0] > 1 or v[1] > 1
    ]

    return dict(
        schema="dsrrl.v282.source_candidate_prebind.v1",
        status="OFFLINE_SOURCE_CANDIDATES_ONLY_NOT_GPU_AUTHORIZED",
        inputs=dict(v12_provenance=True, mtd_donors=True,
                    october10_inventory_supplied=inv is not None),
        counts=dict(spec_name_records=len(spec), diffuse_pairs=len(diffuse),
                    normal_triples=len(normal), mtd_donors=len(materials),
                    ambiguous_spec_name_routes=len(anomalies),
                    diffuse_pairs_not_in_spec_name_table=len(diffuse_without_spec),
                    normal_triples_not_in_spec_name_table=len(normal_without_spec),
                    ambiguous_unlisted_spec_hashes=len(ambiguous_missing),
                    inventory_entries=len(inv) if inv is not None else None,
                    inventory_spec_matches=sum(
                        bool(x.get("inventory_present")) for x in by_spec.values())
                        if inv is not None else None),
        ambiguous_spec_names=anomalies,
        ambiguous_unlisted_spec_hashes=ambiguous_missing,
        diffuse_without_spec_name_route=diffuse_without_spec,
        normal_without_spec_name_route=normal_without_spec,
        texture_candidate_index=by_spec,
        independent_mtd_donors=materials,
        activation_authorized=False,
        activation_requires=[
            "exact live FLVER SHA256 + material slot + exact raw MTD SHA256",
            "exact receiver shader SHA and compatible input/output signatures",
            "exact currently bound t0/t1/t2 logical/resource identity and lifetime",
            "DDS bytes SHA256 and live PTDE companion SRV with valid lifetime",
            "receiver/family-specific complete PTDE material response + EnvSpec operator",
            "correct A/B beta and CB/sampler ABI and downstream composition",
            "all members ready in single draw transaction; otherwise stock fail-open",
        ])


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", type=Path, default=Path("."))
    ap.add_argument("--inventory", type=Path)
    ap.add_argument("--output", required=True, type=Path)
    args = ap.parse_args()
    result = compile_index(args.root, args.inventory)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(result["counts"], sort_keys=True))
    print("BRIDGE_ACTIVATION=OFF / SOURCE_CANDIDATES_ONLY")


if __name__ == "__main__":
    main()
