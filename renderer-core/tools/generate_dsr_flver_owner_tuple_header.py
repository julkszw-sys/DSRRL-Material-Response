#!/usr/bin/env python3
"""Generate the exact DSR FLVER/material owner authenticator header.

Input is a compact, content-addressed zlib payload committed in the repository.
The payload contains one full raw-FLVER SHA-256 per material-owning FLVER and
one exact MTD basename FNV-1a64 per material slot. The complete raw identity is
never truncated.

This is static authorization evidence only. Runtime activation still requires
transporting the actual live raw FLVER SHA-256 + material slot + exact MTD
basename identity to the consumer.
"""
from __future__ import annotations

import argparse
import hashlib
import struct
import zlib
from pathlib import Path

MAGIC = b"DSROWN1\0"
VERSION = 1
EXPECTED_COMPRESSED_SHA256 = (
    "8c711ebf5e5ea55f0b801c34e620f097f109200e69ad568370fa55c5f86e4026"
)
EXPECTED_RAW_SHA256 = (
    "0387ae7955558b15f966df7a58c0731b80a7aa5fb425fba8f8e88c89924eb284"
)
EXPECTED_DSR_ZIP_SHA256 = (
    "a9a2e0eb48625fc735dfe18f7f375105cc5f56be005022969300c778489d0891"
)
EXPECTED_CORPUS_JSONL_SHA256 = (
    "0be97e628b52b74896697bd641c23347d532dd4a0d42d9158b783f22f9d191a5"
)
EXPECTED_GROUP_COUNT = 3944
EXPECTED_TUPLE_COUNT = 19985

HEADER_SIZE = 8 + 16 + 32 + 32
GROUP_SIZE = 32 + 4 + 4


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def load_payload(path: Path):
    compressed = path.read_bytes()
    got = sha256_bytes(compressed)
    if got != EXPECTED_COMPRESSED_SHA256:
        raise ValueError(
            f"compressed owner corpus SHA mismatch: expected "
            f"{EXPECTED_COMPRESSED_SHA256}, got {got}"
        )

    raw = zlib.decompress(compressed)
    got_raw = sha256_bytes(raw)
    if got_raw != EXPECTED_RAW_SHA256:
        raise ValueError(
            f"raw owner corpus SHA mismatch: expected {EXPECTED_RAW_SHA256}, "
            f"got {got_raw}"
        )
    if len(raw) < HEADER_SIZE or raw[:8] != MAGIC:
        raise ValueError("invalid owner corpus magic/header")

    version, group_count, tuple_count, reserved = struct.unpack_from(
        "<IIII", raw, 8
    )
    if version != VERSION or reserved != 0:
        raise ValueError(
            f"unsupported owner corpus version/reserved: {version}/{reserved}"
        )
    if group_count != EXPECTED_GROUP_COUNT:
        raise ValueError(
            f"owner FLVER group invariant mismatch: {group_count}"
        )
    if tuple_count != EXPECTED_TUPLE_COUNT:
        raise ValueError(
            f"owner material tuple invariant mismatch: {tuple_count}"
        )

    dsr_zip_sha = raw[24:56].hex()
    jsonl_sha = raw[56:88].hex()
    if dsr_zip_sha != EXPECTED_DSR_ZIP_SHA256:
        raise ValueError("embedded DSR compact census SHA mismatch")
    if jsonl_sha != EXPECTED_CORPUS_JSONL_SHA256:
        raise ValueError("embedded owner JSONL corpus SHA mismatch")

    expected_size = (
        HEADER_SIZE + group_count * GROUP_SIZE + tuple_count * 8
    )
    if len(raw) != expected_size:
        raise ValueError(
            f"owner corpus length mismatch: {len(raw)} != {expected_size}"
        )

    groups = []
    off = HEADER_SIZE
    expected_first = 0
    previous_sha = None
    for _ in range(group_count):
        digest = raw[off:off + 32]
        first, count = struct.unpack_from("<II", raw, off + 32)
        off += GROUP_SIZE
        if digest == bytes(32):
            raise ValueError("zero FLVER SHA in exact owner corpus")
        if previous_sha is not None and digest <= previous_sha:
            raise ValueError("owner FLVER SHA groups are not strictly sorted")
        if first != expected_first or count == 0:
            raise ValueError(
                f"non-contiguous owner group: first={first}, "
                f"expected={expected_first}, count={count}"
            )
        if first + count > tuple_count:
            raise ValueError("owner group exceeds tuple array")
        groups.append((digest, first, count))
        previous_sha = digest
        expected_first += count

    if expected_first != tuple_count:
        raise ValueError(
            f"owner group coverage mismatch: {expected_first} != {tuple_count}"
        )

    hashes = list(struct.unpack_from(
        f"<{tuple_count}Q", raw, off
    ))
    if any(value == 0 for value in hashes):
        raise ValueError("zero MTD semantic hash in exact owner corpus")

    return groups, hashes


def digest_initializer(digest: bytes) -> str:
    return "{{" + ",".join(f"0x{x:02x}u" for x in digest) + "}}"


def render(groups, hashes) -> str:
    out = [
        "#pragma once\n",
        "#include <array>\n#include <cstddef>\n#include <cstdint>\n\n",
        "namespace dsrrl::operators::material_response::generated {\n",
        "struct flver_owner_group_record {\n",
        "    std::array<std::uint8_t,32> flver_sha256;\n",
        "    std::uint32_t first_material;\n",
        "    std::uint32_t material_count;\n",
        "};\n",
        f'inline constexpr char k_dsr_flver_owner_tuple_source_sha256[]='
        f'"{EXPECTED_DSR_ZIP_SHA256}";\n',
        f'inline constexpr char k_dsr_flver_owner_tuple_corpus_sha256[]='
        f'"{EXPECTED_CORPUS_JSONL_SHA256}";\n',
        "inline constexpr bool k_dsr_flver_owner_tuple_source_complete=true;\n",
        f"inline constexpr std::size_t k_dsr_flver_owner_group_count="
        f"{len(groups)}u;\n",
        f"inline constexpr std::size_t k_dsr_flver_owner_tuple_count="
        f"{len(hashes)}u;\n",
        "inline constexpr std::array<flver_owner_group_record,"
        "k_dsr_flver_owner_group_count> k_dsr_flver_owner_groups = {{\n",
    ]
    for digest, first, count in groups:
        out.append(
            "    {"
            + digest_initializer(digest)
            + f",{first}u,{count}u"
            + "},\n"
        )
    out.extend([
        "}};\n",
        "inline constexpr std::array<std::uint64_t,"
        "k_dsr_flver_owner_tuple_count> k_dsr_flver_owner_mtd_hashes = {{\n",
    ])
    for value in hashes:
        out.append(f"    0x{value:016x}ull,\n")
    out.extend([
        "}};\n",
        "constexpr int compare_digest("
        "const std::array<std::uint8_t,32>&a,"
        "const std::array<std::uint8_t,32>&b) noexcept{"
        "for(std::size_t i=0;i<a.size();++i){"
        "if(a[i]<b[i])return -1;if(a[i]>b[i])return 1;}return 0;}\n",
        "constexpr bool dsr_flver_owner_mtd_hash("
        "const std::array<std::uint8_t,32>&sha,"
        "std::uint32_t slot,std::uint64_t&out) noexcept{"
        "out=0u;"
        "std::size_t lo=0,hi=k_dsr_flver_owner_groups.size();"
        "while(lo<hi){const auto mid=lo+(hi-lo)/2u;"
        "const auto&r=k_dsr_flver_owner_groups[mid];"
        "const int dc=compare_digest(r.flver_sha256,sha);"
        "if(dc<0)lo=mid+1u;else hi=mid;}"
        "if(lo>=k_dsr_flver_owner_groups.size())return false;"
        "const auto&r=k_dsr_flver_owner_groups[lo];"
        "if(compare_digest(r.flver_sha256,sha)!=0||slot>=r.material_count)"
        "return false;"
        "out=k_dsr_flver_owner_mtd_hashes[r.first_material+slot];"
        "return out!=0u;}\n",
        "constexpr bool dsr_flver_owner_tuple_authenticated("
        "const std::array<std::uint8_t,32>&sha,"
        "std::uint32_t slot,std::uint64_t mtd) noexcept{"
        "if(mtd==0u)return false;"
        "std::uint64_t observed=0u;"
        "return dsr_flver_owner_mtd_hash(sha,slot,observed)&&observed==mtd;}\n",
        "} // namespace dsrrl::operators::material_response::generated\n",
    ])
    return "".join(out)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--input", type=Path, required=True)
    ap.add_argument("--output", type=Path)
    ap.add_argument("--verify-only", action="store_true")
    args = ap.parse_args()

    groups, hashes = load_payload(args.input)
    if args.verify_only:
        print(
            f"DSR_OWNER_CORPUS_PASS groups={len(groups)} tuples={len(hashes)} "
            f"source={EXPECTED_DSR_ZIP_SHA256} "
            f"corpus={EXPECTED_CORPUS_JSONL_SHA256}"
        )
        return 0

    if args.output is None:
        ap.error("--output is required unless --verify-only is used")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(render(groups, hashes), encoding="utf-8", newline="\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
