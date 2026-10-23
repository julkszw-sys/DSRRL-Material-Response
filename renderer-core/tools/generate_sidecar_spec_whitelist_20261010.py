#!/usr/bin/env python3
"""Reproduce all 1295 PTDE Specular logical-name identities from 2026-10-10
user-uploaded sidecar inventory. This is *resource-name* coverage ONLY:
MTD routing, full FLVER+slot owner, shader consumer and native D3D11 t1
remain independent gates. Does not grant blanket EnvSpec/PBL authority.

This repository carries a compact zlib+Base85, fully checksum-pinned ASCII
name corpus. The original uploaded _manifests.zip SHA-256 is
1197a9c54b5a0914d3723670a157f3a0ab04559081f38d89bba1bedda3784d3d.
The replayable full names can be emitted with --names-out.
"""
from __future__ import annotations
import argparse
import base64
import hashlib
from pathlib import Path
import re
import zlib

EXPECTED_COUNT = 1295
EXPECTED_NAMES_SHA = "6f417110f6085ca8389213578eb9c0a86c100070717a9df20d62790052c151e4"
EXPECTED_PREFIXES = ("am_", "bd_", "lg_", "hd_", "wp_")
NAME_RE = re.compile(r"^[a-z0-9_]+_s$", re.ASCII)


def fnv1a_ascii_lower(name: str) -> int:
    h = 14695981039346656037
    for byte in name.encode("ascii"):
        h = (h ^ byte) * 1099511628211 & 0xFFFFFFFFFFFFFFFF
    return h


def load_names(source: Path) -> list[str]:
    encoded = "".join(source.read_text(encoding="ascii").split())
    raw = zlib.decompress(base64.b85decode(encoded))
    if hashlib.sha256(raw).hexdigest() != EXPECTED_NAMES_SHA:
        raise ValueError("Sidecar full-name corpus digest mismatch")
    lines = raw.decode("ascii").splitlines()
    if len(lines) != EXPECTED_COUNT or lines != sorted(set(lines)):
        raise ValueError("Expected exactly 1295 unique sorted sidecar names")
    for name in lines:
        if name != name.lower() or not NAME_RE.fullmatch(name) or not name.startswith(EXPECTED_PREFIXES):
            raise ValueError("Invalid non-equipment or non-Specular sidecar name: " + name)
    hashes = [fnv1a_ascii_lower(name) for name in lines]
    if len(set(hashes)) != EXPECTED_COUNT:
        raise ValueError("FNV64 collision among exact sidecar names")
    return lines


def render_header(names: list[str]) -> str:
    values = sorted(fnv1a_ascii_lower(n) for n in names)
    # Source V295/V301 exact owner: 5 *_l_s direct to DSR; 544 not.
    # Keep PTDE low-LOD and unresolved names in the original 1295 corpus,
    # but do not positively authorize them as DSR direct host resources.
    # Five literal *_l_s names remain DIRECTLY bound to original DSR
    # FLVER g_Specular (not to a guessed base alias); see owner handoff CSV.
    dsr_direct_low = {
        "am_f_9450_l_s", "am_m_9372_l_s",
        "bd_m_9440_1_l_s", "bd_m_9440_2_l_s",
        "wp_a_0652_l_s",
    }
    ptde_low = [n for n in names if n.endswith("_l_s") and n not in dsr_direct_low]
    unresolved = {"wp_a_0911_s", "wp_a_1004_s"}
    host = [n for n in names if n not in ptde_low and n not in unresolved]
    if (len(ptde_low)!=544 or len(host)!=749 or
        not unresolved.issubset(names) or
        not dsr_direct_low.issubset(names)):
        raise ValueError("V305 FLVER source census exact name split mismatch")
    host_values = sorted(fnv1a_ascii_lower(n) for n in host)
    out = [
        "#pragma once\n",
        "// AUTO-GENERATED: exact 1295 Specular names in 2026-10-10 DSRRL sidecar.\n",
        "// Corpus SHA256: " + EXPECTED_NAMES_SHA + "\n",
        "// RESOURCE WHITELIST ONLY. NOT FLVER/MTD/OPERATOR AUTHORIZATION.\n",
        "#include <array>\n#include <cstddef>\n#include <cstdint>\n\n",
        "namespace dsrrl::runtime::sidecar_20261010 {\n",
        "inline constexpr std::array<std::uint64_t,1295u> k_spec_hashes = {{\n"
    ]
    out.extend(f"    0x{x:016x}ull,\n" for x in values)
    out.extend([
        "}};\n",
        "constexpr bool sorted_unique() noexcept {\n",
        "    for (std::size_t i=1u;i<k_spec_hashes.size();++i)\n",
        "        if (k_spec_hashes[i-1]>=k_spec_hashes[i]) return false;\n",
        "    return true;\n}\n",
        "static_assert(sorted_unique(),\"Sidecar Specular whitelist hash collision\");\n",
        "inline bool spec_allowed(std::uint64_t value) noexcept {\n",
        "    std::size_t lo=0,hi=k_spec_hashes.size();\n",
        "    while (lo<hi) { const auto mid=lo+((hi-lo)>>1u);\n",
        "        if (value<k_spec_hashes[mid]) hi=mid;\n",
        "        else if (value>k_spec_hashes[mid]) lo=mid+1u;\n",
        "        else return true;\n",
        "    }\n    return false;\n}\n",
        "} // namespace dsrrl::runtime::sidecar_20261010\n"
    ])
    original = "".join(out)
    namespace_tail = "} // namespace dsrrl::runtime::sidecar_20261010\n"
    if original.count(namespace_tail) != 1:
        raise ValueError("V305 source generator namespace drift")
    host_header = (
        "// V305: exact source census DSR direct 749 only; not operator authority.\n"
        "// 544 distinct PTDE low-LOD and 2 unresolved kept above in 1295.\n"
        "inline constexpr std::array<std::uint64_t,749u> k_dsr_direct_spec_hashes = {{\n"
        + "".join(f"    0x{value:016x}ull,\n" for value in host_values)
        + "}};\n"
        "constexpr bool v305_sorted() noexcept {\n"
        "  for (std::size_t i=1u;i<k_dsr_direct_spec_hashes.size();++i)\n"
        "    if(k_dsr_direct_spec_hashes[i-1]>=k_dsr_direct_spec_hashes[i])return false;\n"
        "  return true;\n"
        "}\n"
        'static_assert(v305_sorted(),"V305 host direct Spec hash collision");\n'
        "inline bool dsr_direct_spec_allowed(std::uint64_t v) noexcept {\n"
        "  std::size_t lo=0u, hi=k_dsr_direct_spec_hashes.size();\n"
        "  while(lo<hi){const auto mid=lo+((hi-lo)>>1u);\n"
        "    if(v<k_dsr_direct_spec_hashes[mid])hi=mid;\n"
        "    else if(v>k_dsr_direct_spec_hashes[mid])lo=mid+1u;\n"
        "    else return true;\n"
        "  }\n"
        "  return false;\n"
        "}\n"
    )
    return original.replace(namespace_tail, host_header+namespace_tail)


def main() -> None:
    parser=argparse.ArgumentParser()
    parser.add_argument("--input",type=Path,required=True)
    parser.add_argument("--output",type=Path,required=True)
    parser.add_argument("--names-out",type=Path)
    parser.add_argument("--verify",action="store_true")
    args=parser.parse_args()
    names=load_names(args.input)
    header=render_header(names)
    if args.verify:
        if args.output.read_text(encoding="utf-8") != header:
            raise ValueError("Generated sidecar header drift")
    else:
        args.output.parent.mkdir(parents=True,exist_ok=True)
        args.output.write_text(header,encoding="utf-8",newline="\n")
    if args.names_out is not None:
        args.names_out.write_text("\n".join(names)+"\n",encoding="ascii")
    print("SIDECAR_SPEC_1295_PASS names=1295 collisions=0 sha="+EXPECTED_NAMES_SHA)
    print("RESOURCE_ONLY_NOT_MATERIAL_AUTHORITY")


if __name__=="__main__":
    main()
