#!/usr/bin/env python3
"""Verify exact v2.0.3-dev experimental metal authorizations against two
independent source authorities; no runtime or PTDE pixel claim.

The identities are not guessed from MTD suffixes or shader similarity.
The output of this checker constrains a disabled-by-default operator island
whose actual SRV, selector, receiver and shader preimages still fail open.
"""
from __future__ import annotations

import json
import pathlib
import re

ROOT = pathlib.Path(__file__).resolve().parents[1]

PROFILES = (
    ("P_Metal[DSB].mtd", 345, "pmetal_baseline"),
    ("P_Metal[DSB]_Alp.mtd", 2, "pmetal_alp"),
    ("P_Metal[DSB]_Edge.mtd", 5, "pmetal_edge"),
    ("C_Metal[DSB].mtd", 229, "cmetal"),
)
EXPECTED_SPC = r"FRPG_Phn_ColDifSpcBmp.spx"


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> None:
    routes = json.loads(
        (ROOT / "data/material_response_routes_v1.json").read_text(
            encoding="utf-8"
        )
    )["routes"]
    envspec = json.loads(
        (ROOT / "data/census/ptde_mtd_envspec_router_v1.json").read_text(
            encoding="utf-8"
        )
    )["records"]
    header = (
        ROOT / "include/dsrrl/runtime/ptde_metal_envspec_authority.hpp"
    ).read_text(encoding="utf-8")
    source = (
        ROOT / "src/runtime/pmetal_env_source_runtime.cpp"
    ).read_text(encoding="utf-8")
    consumer = (
        ROOT / "src/runtime/pmetal_envspec_draw_runtime.cpp"
    ).read_text(encoding="utf-8")
    selector_hook = (
        ROOT / "src/runtime/flver_engine_hooks.cpp"
    ).read_text(encoding="utf-8")
    cmake = (ROOT / "integrated/CMakeLists.txt").read_text(encoding="utf-8")
    expected = []

    for name, route, symbol in PROFILES:
        matches = [
            row for row in routes
            if row["mtd_name"] == name and row["route_index"] == route
        ]
        require(len(matches) == 1, f"{name}: unique exact MR route missing")
        mr = matches[0]

        em = [row for row in envspec if row["mtd_name"] == name]
        require(len(em) == 1, f"{name}: unique exact EnvSpec identity missing")
        env = em[0]

        require(mr["sha256"] == env["dsr_mtd_sha256"],
                f"{name}: two independent raw MTD digests disagree")
        require(mr["material_family"] == "DifSpcBmp",
                f"{name}: unverified receiver family")
        require(mr["receiver_triplet"] == "33,34,35",
                f"{name}: unverified stable receiver triplet")
        require(float(mr["c101"]) == 2.5,
                f"{name}: original source c101 no longer applies")
        require(env["state"] == "PRESENT" and env["envspc_slot"] == 2,
                f"{name}: PTDE EnvSpec slot2 not certified")
        require(env["homology_class"] == "HOMOLOGOUS_SPC" and
                env["delta_kind"] == "EXACT_SPX",
                f"{name}: shader semantic homology missing")
        for field in ("dsr_spx", "ptde_spx"):
            require(env[field].endswith(EXPECTED_SPC),
                    f"{name}: {field} differs from exact SPC family")

        # Require the actual header record, including original SHA, not
        # merely the name appearing in a comment or changelog.
        pattern = (
            rf"\{{ptde_metal_envspec_profile::{re.escape(symbol)},\s*"
            rf"{route}u,\s*\"{re.escape(name)}\",\s*"
            rf"\"{mr['sha256']}\",\s*2\.5f\}}"
        )
        require(re.search(pattern, header) is not None,
                f"{name}: compiled authority does not match source census")
        expected.append(name)

    # The shared SHA+route 5 S_Metal sibling has different semantic name.
    require("S_Metal[DSB]_Edge.mtd" not in header,
            "S_Metal SHA alias must not enter equipment P_Metal authority")
    require("DSRRL_EXPERIMENTAL_OTHER_METAL_PTDE_ENVSPEC" in cmake,
            "missing opt-in build flag")
    require(
        re.search(r"DSRRL_EXPERIMENTAL_OTHER_METAL_PTDE_ENVSPEC\s+"
                  r"\"[^\"]+\"\s+OFF\)", cmake) is not None,
        "new metal operator must remain disabled by default"
    )
    require("pmetal_producer_state_latest(material, epoch, out)" in source,
            "exact material-keyed producer/consumer join not present")
    p = source.index("bool pmetal_env_source_runtime::latest_exact_material(")
    q = source.index("bool pmetal_env_source_runtime::latest(", p)
    require("latest_hook_source(" not in source[p:q],
            "new materials may not inherit unkeyed latest-hook fallback")
    require("is_experimental_ptde_metal_envspec_material(material)" in consumer,
            "draw consumer missing exact experimental material selection")
    require("source_.latest_exact_material(material, source)" in consumer,
            "draw consumer missing exact source gate")
    # A MaterialWorkflow consumer that never reaches the native FLVER selector
    # cannot acquire its exact source. Catch route345-only regressions BEFORE
    # compiling a diagnostic addon. The baseline path must remain available.
    require("identity.route_index == 345u" in selector_hook,
            "baseline route345 selector no longer preserved")
    require("is_experimental_ptde_metal_envspec_material(identity)" in
            selector_hook,
            "experimental MTD was not routed to the real FLVER producer")
    require("pmetal_env_source_selector_event(" in selector_hook,
            "native producer hook dispatch missing")
    require("DSRRL_EXPERIMENTAL_OTHER_METAL_PTDE_ENVSPEC" in selector_hook,
            "new producer must not be globally enabled")

    print(json.dumps({
        "result": "PASS_SOURCE_STATIC_ONLY",
        "profiles": expected,
        "profile_count": len(expected),
        "envspec_slot": 2,
        "consumers": [33, 34, 35],
        "opt_in_default": "OFF",
        "unkeyed_hook_fallback": "FORBIDDEN_NEW_MATERIALS",
        "native_runtime": "OPEN",
        "ptde_pixel_equivalence": "OPEN",
    }, sort_keys=True))


if __name__ == "__main__":
    main()
