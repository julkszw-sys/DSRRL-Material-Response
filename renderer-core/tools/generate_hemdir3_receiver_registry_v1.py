#!/usr/bin/env python3
from __future__ import annotations

import argparse
import csv
import re
from pathlib import Path

SHA_RE = re.compile(r"^[0-9a-f]{64}$")

def rows(path: str):
    with Path(path).open("r", encoding="utf-8", newline="") as f:
        filtered = (line for line in f if not line.startswith("#"))
        return list(csv.DictReader(filtered, delimiter="\t"))

def fail(msg: str) -> None:
    raise SystemExit(msg)

def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--receivers", required=True)
    ap.add_argument("--patch-sites", required=True)
    ap.add_argument("--replacements", required=True)
    ap.add_argument("--out", required=True)
    a = ap.parse_args()

    receiver_rows = rows(a.receivers)
    patch_rows = rows(a.patch_sites)
    replacement_rows = rows(a.replacements)

    if len(receiver_rows) != 48:
        fail(f"expected 48 HemDir3 receivers, got {len(receiver_rows)}")
    if len(patch_rows) != 48 or len(replacement_rows) != 48:
        fail("patch/replacement tables must each contain 48 rows")

    patches_by_sha = {r["code_sha256"]: r for r in patch_rows}
    replacements_by_sha = {r["source_sha256"]: r for r in replacement_rows}

    if len(patches_by_sha) != 48 or len(replacements_by_sha) != 48:
        fail("duplicate source SHA in HemDir3 authority")

    receiver_entries = []
    patch_entries = []
    spc_ids = []

    for plan_index, r in enumerate(receiver_rows):
        sha = r["code_sha256"]
        if not SHA_RE.fullmatch(sha):
            fail(f"bad source sha: {sha}")

        if sha not in patches_by_sha or sha not in replacements_by_sha:
            fail(f"authority join missing for {sha}")

        p = patches_by_sha[sha]
        repl = replacements_by_sha[sha]

        if p["stratum"] != r["stratum"]:
            fail(f"stratum mismatch for {sha}")

        stratum = r["stratum"]
        if stratum not in ("nospc", "spc"):
            fail(f"unsupported stratum {stratum}")

        expected_count = 15 if stratum == "spc" else 9
        patch_count = int(p["patch_count"])
        if patch_count != expected_count:
            fail(f"wrong patch count for {sha}: {patch_count}")

        sites = []
        for item in p["patch_word_sites"].split(","):
            word_s, reg_s = item.split(":", 1)
            word = int(word_s)
            reg = int(reg_s)
            if reg < 92 or reg > 99:
                fail(f"patch reg out of carrier range: {sha} word={word} reg={reg}")
            sites.append((word, reg))

        if len(sites) != patch_count or len({w for w, _ in sites}) != patch_count:
            fail(f"bad patch site cardinality for {sha}")

        regs = [reg for _, reg in sites]
        expected_reg_counts = (
            {92:2,93:2,94:2,95:2,96:2,97:2,98:1,99:2}
            if stratum == "spc"
            else {92:1,93:1,94:1,95:1,96:1,97:1,98:1,99:2}
        )
        actual = {reg: regs.count(reg) for reg in range(92, 100)}
        if actual != expected_reg_counts:
            fail(f"carrier-use shape mismatch for {sha}: {actual}")

        source_size = int(r["code_size"])
        replacement_size = int(repl["replacement_size"])
        if replacement_size != source_size + 16:
            fail(f"replacement size mismatch for {sha}")

        replacement_sha = repl["replacement_sha256"]
        if not SHA_RE.fullmatch(replacement_sha):
            fail(f"bad replacement sha: {replacement_sha}")

        paired_id = int(r["paired_stable_receiver_id"])
        if stratum == "spc":
            if paired_id < 24 or paired_id > 47:
                fail(f"bad paired stable receiver id {paired_id}")
            spc_ids.append(paired_id)
        elif paired_id != 0:
            fail("no-Spc receiver must not borrow stable Spc receiver identity")

        first_patch = len(patch_entries)
        patch_entries.extend(sites)

        receiver_entries.append({
            "plan_index": plan_index,
            "shader_index": int(r["shader_index"]),
            "name": r["shader_name"],
            "stratum": stratum,
            "source_size": source_size,
            "source_sha": sha,
            "replacement_size": replacement_size,
            "replacement_sha": replacement_sha,
            "first_patch": first_patch,
            "patch_count": patch_count,
            "paired_id": paired_id,
        })

    if sorted(spc_ids) != list(range(24, 48)):
        fail(f"Spc paired receiver IDs are not exactly 24..47: {spc_ids}")

    if sum(1 for x in receiver_entries if x["stratum"] == "spc") != 24:
        fail("expected 24 Spc HemDir3 receivers")
    if sum(1 for x in receiver_entries if x["stratum"] == "nospc") != 24:
        fail("expected 24 no-Spc HemDir3 receivers")
    if len(patch_entries) != 576:
        fail(f"expected 576 patch sites, got {len(patch_entries)}")

    out = []
    out += [
        "#pragma once\n",
        "#include <array>\n",
        "#include <cstdint>\n",
        "#include <string_view>\n\n",
        "namespace dsrrl::operators::lightbank::generated {\n\n",
        "enum class hemdir3_generated_stratum : std::uint8_t { nospc = 0, spc = 1 };\n\n",
        "struct hemdir3_b13_patch_site {\n",
        "    std::uint32_t slot_word;\n",
        "    std::uint8_t source_register;\n",
        "};\n\n",
        "struct hemdir3_receiver_plan {\n",
        "    std::uint16_t plan_index;\n",
        "    std::uint16_t shader_index;\n",
        "    std::string_view shader_name;\n",
        "    hemdir3_generated_stratum stratum;\n",
        "    std::uint32_t stock_size;\n",
        "    std::string_view stock_sha256;\n",
        "    std::uint32_t replacement_size;\n",
        "    std::string_view replacement_sha256;\n",
        "    std::uint16_t first_patch;\n",
        "    std::uint8_t patch_count;\n",
        "    std::uint8_t paired_stable_receiver_id;\n",
        "};\n\n",
        f"inline constexpr std::array<hemdir3_b13_patch_site,{len(patch_entries)}> k_hemdir3_b13_patch_sites = {{{{\n",
    ]
    for word, reg in patch_entries:
        out.append(f"    {{{word}u,{reg}u}},\n")
    out += [
        "}};\n\n",
        "inline constexpr std::array<hemdir3_receiver_plan,48> k_hemdir3_receiver_plans = {{\n",
    ]
    for r in receiver_entries:
        enum_value = "hemdir3_generated_stratum::spc" if r["stratum"] == "spc" else "hemdir3_generated_stratum::nospc"
        name = r["name"].replace("\\", "\\\\").replace('"', '\\"')
        out.append(
            f'    {{{r["plan_index"]}u,{r["shader_index"]}u,"{name}",{enum_value},'
            f'{r["source_size"]}u,"{r["source_sha"]}",{r["replacement_size"]}u,'
            f'"{r["replacement_sha"]}",{r["first_patch"]}u,{r["patch_count"]}u,{r["paired_id"]}u}},\n'
        )
    out += [
        "}};\n\n",
        "} // namespace dsrrl::operators::lightbank::generated\n",
    ]

    Path(a.out).write_text("".join(out), encoding="utf-8")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
