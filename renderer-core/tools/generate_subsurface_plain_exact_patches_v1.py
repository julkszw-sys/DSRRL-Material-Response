#!/usr/bin/env python3
import argparse
import hashlib
from pathlib import Path

RECORDS = [
    ("Csd", 33,
     "0b8288d686c8f349ad87352946be51ffd007462f25357326bf47e736e690e511",
     "35880c0b2f2330208dfc21af6dd3d944218fcc4540cd8e59404a0aefc13c0b24",
     "5610f33a208fc7eb723d7b9d83f883ad61c5606a01950e402f64b8d3f708a317",
     21052, 19856),
    ("Sdw", 34,
     "885337e50f3d29f086fd18e1f7524f28712031d0264964aef5f37037df7d7bcb",
     "d6038de494509e7cbcbfb904c4046e9427f3b921f6a35735a0b0d316f9976837",
     "847b762168b8da59146c23b00523bf8f01685f1b4d95cf3fe92de10bae325b08",
     20752, 19556),
    ("Plain", 35,
     "3002cfb9aee6835412399c3be267ab94c5706d7d5030fc4cafc82bc54c55a860",
     "7d03c75b69f5730eb741a4d327189d0bbed8a8450fb0ac04e1505f7b91763701",
     "93e9c7432c5fda2dc26f72a1f51ffd0608b5fea0ae3ad1e34d14a4f00912fe2f",
     19256, 18060),
]

def u16(b, o):
    return b[o] | (b[o + 1] << 8)

def u32(b, o):
    return b[o] | (b[o + 1] << 8) | (b[o + 2] << 16) | (b[o + 3] << 24)

def validate_patch(label, data, patch_sha, source_size, target_size):
    if hashlib.sha256(data).hexdigest() != patch_sha:
        raise SystemExit(f"{label}: patch SHA-256 mismatch")
    if len(data) < 12:
        raise SystemExit(f"{label}: patch too short")
    op_count = u16(data, 0)
    lit_words = u16(data, 2)
    source_words = u32(data, 4)
    target_words = u32(data, 8)
    if source_words * 4 != source_size or target_words * 4 != target_size:
        raise SystemExit(f"{label}: source/target size contract mismatch")
    expected = 12 + op_count * 5 + lit_words * 4
    if expected != len(data):
        raise SystemExit(
            f"{label}: malformed patch payload: {len(data)} != {expected}")
    produced = 0
    for i in range(op_count):
        at = 12 + i * 5
        kind = data[at]
        a = u16(data, at + 1)
        count = u16(data, at + 3)
        if count == 0:
            raise SystemExit(f"{label}: zero-length operation {i}")
        if kind == 0:
            if a + count > source_words:
                raise SystemExit(f"{label}: source copy out of range")
        elif kind == 1:
            if a + count > lit_words:
                raise SystemExit(f"{label}: literal copy out of range")
        else:
            raise SystemExit(f"{label}: unknown operation kind {kind}")
        produced += count
    if produced != target_words:
        raise SystemExit(
            f"{label}: produced words {produced} != target words {target_words}")

def emit_array(name, data):
    lines = []
    for i in range(0, len(data), 16):
        chunk = data[i:i + 16]
        lines.append(
            "        " + ", ".join(f"0x{x:02x}" for x in chunk) + ",")
    return (
        f"inline constexpr std::array<std::uint8_t, {len(data)}u> {name}{{{{\n"
        + "\n".join(lines)
        + "\n}};\n")

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--input-dir", required=True)
    ap.add_argument("--output", required=True)
    args = ap.parse_args()

    root = Path(args.input_dir)
    arrays = []
    rows = []
    for (
        label, receiver, source_sha, target_sha, patch_sha,
        source_size, target_size
    ) in RECORDS:
        data = (root / f"{label}.bin").read_bytes()
        validate_patch(
            label, data, patch_sha, source_size, target_size)
        symbol = f"k_subsurface_plain_patch_{label.lower()}"
        arrays.append(emit_array(symbol, data))
        rows.append((
            label, receiver, source_sha, target_sha, patch_sha,
            source_size, target_size, symbol, len(data)))

    out = [
        "#pragma once",
        "",
        "#include <array>",
        "#include <cstddef>",
        "#include <cstdint>",
        "#include <string_view>",
        "",
        "namespace dsrrl::operators::resource_bridges::generated {",
        "",
        "struct subsurface_plain_patch_record {",
        "    std::string_view variant;",
        "    std::uint32_t target_receiver_id;",
        "    std::string_view source_sha256;",
        "    std::string_view target_sha256;",
        "    std::string_view patch_sha256;",
        "    std::size_t source_size;",
        "    std::size_t target_size;",
        "    const std::uint8_t *patch;",
        "    std::size_t patch_size;",
        "};",
        "",
    ]
    out.extend(arrays)
    out.append(
        "inline constexpr std::array<subsurface_plain_patch_record, 3u> "
        "k_subsurface_plain_patches{{")
    for (
        label, receiver, source_sha, target_sha, patch_sha,
        source_size, target_size, symbol, size
    ) in rows:
        out.append(
            "    subsurface_plain_patch_record{"
            f"\"{label}\", {receiver}u, "
            f"\"{source_sha}\", \"{target_sha}\", \"{patch_sha}\", "
            f"{source_size}u, {target_size}u, "
            f"{symbol}.data(), {size}u"
            "},")
    out.extend([
        "};",
        "",
        "} // namespace dsrrl::operators::resource_bridges::generated",
        "",
    ])

    output = Path(args.output)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text("\n".join(out), encoding="utf-8")

if __name__ == "__main__":
    main()
