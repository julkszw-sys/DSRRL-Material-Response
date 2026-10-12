#!/usr/bin/env python3
"""V245 operator-local producer transport repair over the V237 no-QPC source.

Prevent a synchronized selector record from being re-armed with stale PTDE
EnvDiffuse A/B or PHN content. This is NOT proof that the user's stock-DSR
HemEnvLerp regression is thereby fixed; visual causality remains OPEN.
The R19 shader, receiver routing, resource matching, CB ABI and draw
transaction remain untouched. Run after V236 mutex patch, never mutate the
immutable R43 source donor or disk EXE.
"""
from pathlib import Path
import argparse

SOURCE = Path("renderer-core/src/runtime/pmetal_producer_state.cpp")
TEST = Path("renderer-core/tests/pmetal_producer_state_tests.cpp")

OLD_SAME = """        a.a == b.a &&
        a.b == b.b &&
        a.beta == b.beta &&
"""
NEW_SAME = """        a.a == b.a &&
        a.b == b.b &&
        a.envdiffuse_a == b.envdiffuse_a &&
        a.envdiffuse_b == b.envdiffuse_b &&
        a.envdiffuse_linear_valid == b.envdiffuse_linear_valid &&
        a.phn_k135 == b.phn_k135 &&
        a.phn_k135_valid == b.phn_k135_valid &&
        a.beta == b.beta &&
"""
OLD_KEY = """    for (const auto value : source.b)
        hash = fnv_scalar(hash, value);
    hash = fnv_scalar(hash, source.beta);
"""
NEW_KEY = """    for (const auto value : source.b)
        hash = fnv_scalar(hash, value);
    for (const auto value : source.envdiffuse_a)
        hash = fnv_scalar(hash, value);
    for (const auto value : source.envdiffuse_b)
        hash = fnv_scalar(hash, value);
    hash = fnv_scalar(hash, source.envdiffuse_linear_valid);
    hash = fnv_scalar(hash, source.phn_k135);
    hash = fnv_scalar(hash, source.phn_k135_valid);
    hash = fnv_scalar(hash, source.beta);
"""
OLD_REARM = """        slot.payload_key.load(
            std::memory_order_acquire) == payload) {
        slot.valid.store("""
NEW_REARM = """        slot.payload_key.load(
            std::memory_order_acquire) == payload &&
        // The FNV key is a fast discriminator, NOT equality authority.
        // Never re-arm a record whose actual downstream payload differs.
        // V236 holds slot.mutex across this check and the update.
        slot.record.valid &&
        slot.record.epoch == epoch &&
        same_material(slot.record.material, material) &&
        same_source_payload(slot.record.source, g_record.source) &&
        slot.record.generation == g_record.generation) {
        slot.valid.store("""
TEST_ANCHOR = """    return 0;
}
"""
TEST_BODY = """    // V245 positive and negative control, INCLUDING under Release/NDEBUG.
    // Same source A/B and beta, but independent PTDE EnvDiffuse/PHN inputs
    // change between published synchronized selector records.
    source.envdiffuse_linear_valid = true;
    source.envdiffuse_a = {0.4f, 0.5f, 0.6f};
    source.envdiffuse_b = {0.7f, 0.8f, 0.9f};
    source.phn_k135_valid = true;
    source.phn_k135 = 1.25f;
    runtime::pmetal_producer_state_begin(material, 100u);
    runtime::pmetal_producer_state_publish(material, source, 100u);
    runtime::pmetal_producer_state_clear();
    runtime::pmetal_envspec_source first{};
    if (!runtime::pmetal_producer_state_latest(material, 100u, first))
        return 41;
    if (first.envdiffuse_a[0] != 0.4f || first.phn_k135 != 1.25f)
        return 42;

    source.envdiffuse_a[0] = 0.25f;
    source.envdiffuse_b[2] = 0.35f;
    source.phn_k135 = 1.5f;
    runtime::pmetal_producer_state_begin(material, 100u);
    runtime::pmetal_producer_state_publish(material, source, 100u);
    runtime::pmetal_producer_state_clear();
    runtime::pmetal_envspec_source second{};
    if (!runtime::pmetal_producer_state_latest(material, 100u, second))
        return 44;
    if (second.envdiffuse_a[0] != 0.25f ||
        second.envdiffuse_b[2] != 0.35f ||
        second.phn_k135 != 1.5f)
        return 43;
    if (second.generation <= first.generation)
        return 45;

    // Changes in the valid bits must also force a fresh payload.
    source.envdiffuse_linear_valid = false;
    source.phn_k135_valid = false;
    runtime::pmetal_producer_state_begin(material, 100u);
    runtime::pmetal_producer_state_publish(material, source, 100u);
    runtime::pmetal_producer_state_clear();
    runtime::pmetal_envspec_source third{};
    if (!runtime::pmetal_producer_state_latest(material, 100u, third))
        return 46;
    if (third.envdiffuse_linear_valid || third.phn_k135_valid ||
        third.generation <= second.generation)
        return 47;

    // Identical payload must keep a stable semantic generation.
    runtime::pmetal_producer_state_begin(material, 100u);
    runtime::pmetal_producer_state_publish(material, source, 100u);
    runtime::pmetal_producer_state_clear();
    runtime::pmetal_envspec_source same{};
    if (!runtime::pmetal_producer_state_latest(material, 100u, same))
        return 48;
    if (same.generation != third.generation ||
        same.envdiffuse_a != third.envdiffuse_a ||
        same.envdiffuse_b != third.envdiffuse_b)
        return 49;

    return 0;
}
"""

def strict_replace(s: str, old: str, new: str) -> str:
    if s.count(new) == 1 and s.count(old) == 0:
        return s
    if s.count(old) != 1 or s.count(new) != 0:
        raise RuntimeError("V245 exact source preimage drift: " + old[:64])
    return s.replace(old, new, 1)

def patch_source(s: str) -> str:
    # V236 mutex required: no unguarded dereference of slot.record allowed.
    before = """    auto &slot = sync_slot_for(key);
    std::lock_guard<std::mutex> lock(slot.mutex);

    // Same exact material/source/epoch:"""
    if s.count(before) != 1:
        raise RuntimeError("V245 requires applied V236 shared-slot mutex")
    for old, new in ((OLD_SAME, NEW_SAME),
                     (OLD_KEY, NEW_KEY),
                     (OLD_REARM, NEW_REARM)):
        s = strict_replace(s, old, new)
    assert s.count("slot.record = g_record;") == 1
    assert s.count("if (pmetal_producer_state_latest(") == 0
    return s

def patch_test(s: str) -> str:
    if s.count(TEST_BODY) == 1:
        return s
    if s.count(TEST_ANCHOR) != 1:
        raise RuntimeError("V245 existing producer unit test drift")
    return s.replace(TEST_ANCHOR, TEST_BODY, 1)

def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--apply", action="store_true")
    ap.add_argument("--tests", action="store_true")
    args = ap.parse_args()
    target = TEST if args.tests else SOURCE
    source = target.read_text(encoding="utf-8")
    out = patch_test(source) if args.tests else patch_source(source)
    if args.apply and out != source:
        target.write_text(out, encoding="utf-8")
    elif not args.apply and out != source:
        raise RuntimeError("V245 overlay not materialized: " + str(target))
    print("V245_EXACT_SOURCE_RECORD_REARM_" +
          ("APPLIED" if args.apply else "VERIFIED"))

if __name__ == "__main__":
    main()
