#!/usr/bin/env python3
"""V259 exact-stock DSR Fog/LightScattering opcode-region guard.

Source overlay ONLY. Inject into the existing create-time R19 HemEnvLerp
materializer, never into Fog, CB0, FogBank, late PHN, or latest_hook_source.
A candidate whose native Fog/LS region is not byte-identical to the donor
is dropped, so the create-time replacement fails open to stock DSR.

The constants below are derived independently from the original user-supplied
DSR FRPG_FlverPBL_fpo_DX11 shaderbnd.dcx, exact SHA-matched RX33/34/35.
No retail shader bytes are checked into this repository.
"""
import argparse
from pathlib import Path

SOURCE = Path("renderer-core/src/operators/env_spec/pmetal_rgba_lerp_materializer.cpp")
CUT = "// V259 EXACT STOCK DSR FOG/LS GUARD: 212 DWORDS"
INSERT_BEFORE = "} // namespace\n\npmetal_rgba_lerp_materialize_outcome"
INSERT_POST = """    outcome.result =
        pmetal_rgba_lerp_materialize_result::applied;
    return outcome;"""
POST_NEW = """    // V259: the separate material response may be modified, but the
    // ENTIRE certified Fog/LightScattering consumer window remains stock.
    // Before returning any create-time shader replacement, enforce the
    // exact original RX33/34/35 opcode subchain. Failure => stock DSR.
    if (!v259_stock_dsr_fog_ls_guard(
            stock_source, stock_size, output.data(), output.size(),
            site->terminal_rgb_word)) {
        output.clear();
        outcome.result =
            pmetal_rgba_lerp_materialize_result::fail_postcondition;
        return outcome;
    }
""" + INSERT_POST

HELPER = r"""// V259 EXACT STOCK DSR FOG/LS GUARD: 212 DWORDS
// RX33: [2552,2764), terminal=2904
// RX34: [2471,2683), terminal=2823
// RX35: [2124,2336), terminal=2476
// All original stock DWORD windows have SHA256
// bf52da4e8b27d14b56cd047e67d8b012b2824309c7b55ca64e9ba536940286b0.
// The first instruction consumes cb0[12] Fog. The end is the last
// instruction with a cb0[12..20] atmospheric constant. Terminal instructions
// after the boundary are deliberately NOT part of this restricted assertion.
//
// This is create-time exact-original-subsequence attestation. The patched
// DXBC must have the exact original 212 DWORDs at a unique location.
// This check does not itself prove DSR Fog CB0 uploads, PTDE pixel parity,
// or that unverified material draws may be patched.
bool v259_stock_dsr_fog_ls_guard(
    const std::uint8_t *stock,
    std::size_t stock_size,
    const std::uint8_t *candidate,
    std::size_t candidate_size,
    std::uint32_t terminal_word) noexcept
{
    if (!stock || !candidate || stock_size == 0u ||
        candidate_size == 0u ||
        terminal_word < 352u)
        return false;

    std::vector<chunk> original_chunks, candidate_chunks;
    std::vector<std::uint32_t> orig, modified;
    std::size_t orig_code = 0u, cand_code = 0u;
    if (!parse_dxbc(
            stock, stock_size,
            original_chunks, orig_code, orig) ||
        !parse_dxbc(
            candidate, candidate_size,
            candidate_chunks, cand_code, modified))
        return false;

    constexpr std::size_t k_212_words = 212u;
    const std::size_t start =
        static_cast<std::size_t>(terminal_word) - 352u;
    const std::size_t end =
        static_cast<std::size_t>(terminal_word) - 140u;
    if (end - start != k_212_words ||
        start < 2u || end > orig.size() ||
        modified.size() < k_212_words + 2u)
        return false;

    const auto digest = hashing::sha256(
        reinterpret_cast<const std::uint8_t *>(
            orig.data() + start),
        k_212_words * sizeof(std::uint32_t));
    if (!hashing::matches_hex(
            digest,
            "bf52da4e8b27d14b56cd047e67d8b012b2824309c7b55ca64e9ba536940286b0"))
        return false;

    // No opcode change, no cb0[12..20] operand rewriting, no reordered
    // instructions anywhere between Fog and the last LightScattering
    // CB consumer. Enforce a unique contiguous occurrence.
    const auto begin = orig.begin() +
        static_cast<std::ptrdiff_t>(start);
    const auto finish = orig.begin() +
        static_cast<std::ptrdiff_t>(end);
    const auto first = std::search(
        modified.begin() + 2, modified.end(), begin, finish);
    if (first == modified.end())
        return false;
    const auto repeated = std::search(
        first + 1, modified.end(), begin, finish);
    return repeated == modified.end();
}

"""

def require_once(s, target, label):
    if s.count(target) != 1:
        raise RuntimeError(
            f"V259 {label}: preimage occurs {s.count(target)} times, expected 1")

def transform(source):
    if source.count(CUT) == 1 and POST_NEW in source:
        return source
    if CUT in source or "v259_stock_dsr_fog_ls_guard(" in source:
        raise RuntimeError("V259 partial overlay; fail open")
    require_once(source, INSERT_BEFORE, "anonymous namespace")
    require_once(source, INSERT_POST, "materializer terminal")
    source = source.replace(INSERT_BEFORE, HELPER + INSERT_BEFORE, 1)
    source = source.replace(INSERT_POST, POST_NEW, 1)
    assert source.count(CUT) == 1
    return source

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--apply", action="store_true")
    args = ap.parse_args()
    data = SOURCE.read_text(encoding="utf8").replace("\r\n","\n")
    result = transform(data)
    if args.apply:
        if result != data:
            SOURCE.write_text(result, encoding="utf8", newline="\n")
        assert transform(result) == result
    elif data != result:
        raise RuntimeError("V259 overlay not yet applied")
    print("V259_STOCK_DSR_FOG_LS_OPCODE_GUARD_" +
          ("APPLIED" if args.apply else "PASS"))

if __name__=="__main__":
    main()
