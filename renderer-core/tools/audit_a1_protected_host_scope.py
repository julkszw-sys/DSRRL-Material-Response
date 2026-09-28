#!/usr/bin/env python3
import argparse
import json
import re
from pathlib import Path

EXPECTED_PLANS = 144
EXPECTED_ALIASES = 252
EXPECTED_ENVSPEC_NOSPC_PLANS = 12
EXPECTED_ENVSPEC_NOSPC_ALIASES = 24
EXPECTED_BUILD151_NOSPC = 24
EXPECTED_TOTAL_NOSPC_IDENTITIES = 48
PROTECTED = re.compile(r"^FRPG_Phn_.*Spc")

def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--index", required=True)
    ap.add_argument(
        "--build151",
        default="renderer-core/include/dsrrl/operators/legacy_plan/build151_nospc_extension.hpp",
    )
    args = ap.parse_args()

    doc = json.loads(
        Path(args.index).read_text(encoding="utf-8")
        .replace("\\n", "")
    )
    plans = doc["plans"]

    if len(plans) != EXPECTED_PLANS:
        raise SystemExit(
            f"expected {EXPECTED_PLANS} plans, got {len(plans)}"
        )

    aliases = sum(len(p["aliases"]) for p in plans)
    if aliases != EXPECTED_ALIASES:
        raise SystemExit(
            f"expected {EXPECTED_ALIASES} aliases, got {aliases}"
        )

    protected = []
    for plan in plans:
        names = [plan["representative"], *plan["aliases"]]
        for name in names:
            if PROTECTED.search(name):
                protected.append(name)

    if protected:
        raise SystemExit(
            "A1 create-time corpus entered protected Phn Spc hosts: "
            + ", ".join(sorted(set(protected)))
        )

    envspec_nospc = [
        p for p in plans
        if "envspec_nospc_delete" in p.get("owners", [])
    ]
    envspec_nospc_aliases = sum(
        len(p.get("aliases", [])) for p in envspec_nospc
    )
    if len(envspec_nospc) != EXPECTED_ENVSPEC_NOSPC_PLANS:
        raise SystemExit(
            "EnvSpec no-Spc exact A1 coverage drifted: "
            f"expected {EXPECTED_ENVSPEC_NOSPC_PLANS} plans, "
            f"got {len(envspec_nospc)}"
        )
    if envspec_nospc_aliases != EXPECTED_ENVSPEC_NOSPC_ALIASES:
        raise SystemExit(
            "EnvSpec no-Spc exact A1 alias coverage drifted: "
            f"expected {EXPECTED_ENVSPEC_NOSPC_ALIASES}, "
            f"got {envspec_nospc_aliases}"
        )
    if any(not p.get("replacement_sha256") for p in envspec_nospc):
        raise SystemExit("EnvSpec no-Spc A1 plan missing replacement SHA-256")

    a1_nospc_names = sorted({
        name
        for p in envspec_nospc
        for name in p.get("aliases", [])
    })
    if len(a1_nospc_names) != EXPECTED_ENVSPEC_NOSPC_ALIASES:
        raise SystemExit(
            "EnvSpec no-Spc A1 alias identities are not unique: "
            f"expected {EXPECTED_ENVSPEC_NOSPC_ALIASES}, got {len(a1_nospc_names)}"
        )

    build151_text = Path(args.build151).read_text(encoding="utf-8")
    build151_names = []
    build151_entry = re.compile(
        r'\\{"(FRPG_Phn_[^"]+)","[0-9a-f]{64}",\\d+u,\\d+u,"[0-9a-f]{64}"\\},?'
    )
    for line in build151_text.splitlines():
        match = build151_entry.fullmatch(line.strip())
        if match:
            build151_names.append(match.group(1))

    if len(build151_names) != EXPECTED_BUILD151_NOSPC:
        raise SystemExit(
            "Build151 EnvSpec no-Spc exact identity coverage drifted: "
            f"expected {EXPECTED_BUILD151_NOSPC}, got {len(build151_names)}"
        )
    if len(set(build151_names)) != EXPECTED_BUILD151_NOSPC:
        raise SystemExit("Build151 EnvSpec no-Spc identity names are not unique")
    if sum("HemEnvLerp.fpo" in n for n in build151_names) != 12:
        raise SystemExit("Build151 EnvSpec no-Spc HemEnvLerp coverage must be 12")
    if sum(n.endswith("HemEnv.fpo") for n in build151_names) != 12:
        raise SystemExit("Build151 EnvSpec no-Spc HemEnv coverage must be 12")
    if set(a1_nospc_names) & set(build151_names):
        raise SystemExit("A1 and Build151 EnvSpec no-Spc identity strata overlap")
    if len(a1_nospc_names) + len(build151_names) != EXPECTED_TOTAL_NOSPC_IDENTITIES:
        raise SystemExit("Combined EnvSpec no-Spc DSR identity coverage must be 48")

    gst_spc = sorted({
        name
        for plan in plans
        for name in [plan["representative"], *plan["aliases"]]
        if name.startswith("FRPG_Gst_") and "Spc" in name
    })

    print(json.dumps({
        "status": "PASS",
        "plans": len(plans),
        "aliases": aliases,
        "protected_phn_spc_hits": 0,
        "envspec_nospc_plans": len(envspec_nospc),
        "envspec_nospc_aliases": envspec_nospc_aliases,
        "envspec_nospc_build151_identities": len(build151_names),
        "envspec_nospc_total_dsr_identities": len(a1_nospc_names) + len(build151_names),
        "gst_spc_names": len(gst_spc),
    }, indent=2))

    return 0

if __name__ == "__main__":
    raise SystemExit(main())
