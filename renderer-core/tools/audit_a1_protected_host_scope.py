#!/usr/bin/env python3
import argparse
import json
import re
from pathlib import Path

EXPECTED_PLANS = 144
EXPECTED_ALIASES = 252
PROTECTED = re.compile(r"^FRPG_Phn_.*Spc")

def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--index", required=True)
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
        "gst_spc_names": len(gst_spc),
    }, indent=2))

    return 0

if __name__ == "__main__":
    raise SystemExit(main())
