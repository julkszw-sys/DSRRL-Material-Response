#!/usr/bin/env python3
"""V242 repair authoritative synchronized producer payload identity.

EnvDiffuse A/B, its valid bit and PHN k135 were omitted from BOTH
same_source_payload and source_key. The same EnvSpec/LightBank selector
could therefore fast-rearm a stale synchronized record while only the
semantic downstream carrier changed. Preserve material key/slot/epoch,
producer-lock semantics and PTDE renderer equations.
"""
from pathlib import Path
import argparse
PATH=Path("renderer-core/src/runtime/pmetal_producer_state.cpp")
OLD_SAME="""        a.a == b.a &&
        a.b == b.b &&
        a.beta == b.beta &&
"""
NEW_SAME="""        a.a == b.a &&
        a.b == b.b &&
        a.envdiffuse_a == b.envdiffuse_a &&
        a.envdiffuse_b == b.envdiffuse_b &&
        a.envdiffuse_linear_valid == b.envdiffuse_linear_valid &&
        a.phn_k135 == b.phn_k135 &&
        a.phn_k135_valid == b.phn_k135_valid &&
        a.beta == b.beta &&
"""
OLD_HASH="""    for (const auto value : source.b)
        hash = fnv_scalar(hash, value);
    hash = fnv_scalar(hash, source.beta);
"""
NEW_HASH="""    for (const auto value : source.b)
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
TESTPATH=Path("renderer-core/tests/pmetal_producer_state_tests.cpp")
OLD_TEST="""    return 0;
}
"""
NEW_TEST="""    // V242: equality/hash MUST include independent EnvDiffuse and PHN.
    // Re-arm with the same exact EnvSpec/LightBank/beta but changed
    // downstream payload, drop selector TLS and read on consumer path.
    source.envdiffuse_linear_valid = true;
    source.envdiffuse_a = {0.4f, 0.5f, 0.6f};
    source.envdiffuse_b = {0.7f, 0.8f, 0.9f};
    source.phn_k135_valid = true;
    source.phn_k135 = 1.25f;
    runtime::pmetal_producer_state_begin(material, 100u);
    runtime::pmetal_producer_state_publish(material, source, 100u);
    runtime::pmetal_producer_state_clear();
    runtime::pmetal_envspec_source first_carrier{};
    assert(runtime::pmetal_producer_state_latest(material, 100u, first_carrier));
    assert(first_carrier.envdiffuse_a[0] == 0.4f);

    source.envdiffuse_a[0] = 0.25f;
    source.envdiffuse_b[2] = 0.35f;
    source.phn_k135 = 1.5f;
    runtime::pmetal_producer_state_begin(material, 100u);
    runtime::pmetal_producer_state_publish(material, source, 100u);
    runtime::pmetal_producer_state_clear();
    runtime::pmetal_envspec_source latest_carrier{};
    assert(runtime::pmetal_producer_state_latest(material, 100u, latest_carrier));
    assert(latest_carrier.envdiffuse_a[0] == 0.25f);
    assert(latest_carrier.envdiffuse_b[2] == 0.35f);
    assert(latest_carrier.phn_k135 == 1.5f);
    assert(latest_carrier.envdiffuse_linear_valid);
    assert(latest_carrier.phn_k135_valid);
    assert(latest_carrier.generation > first_carrier.generation);

    return 0;
}
"""
def patch(s: str, apply: bool=False)->str:
    old=s.count(OLD_SAME)==1 and s.count(OLD_HASH)==1 and NEW_SAME not in s and NEW_HASH not in s
    new=s.count(NEW_SAME)==1 and s.count(NEW_HASH)==1
    if old==new:raise ValueError("V242 source identity precondition invalid")
    for x in ("pmetal_producer_state_begin(", "pmetal_producer_state_publish(",
              "pmetal_producer_state_latest(", "same_material(",
              "source.row_id_b", "slot.record = g_record;"):
        if x not in s:raise ValueError("V242 protected contract missing: "+x)
    if not apply or new:return s
    return s.replace(OLD_SAME,NEW_SAME,1).replace(OLD_HASH,NEW_HASH,1)
def patch_test(s: str, apply: bool=False)->str:
    original=s.count(OLD_TEST)==1 and NEW_TEST not in s
    new=s.count(NEW_TEST)==1
    if original==new:raise ValueError("V242 regression test source drift")
    return s.replace(OLD_TEST,NEW_TEST,1) if apply and original else s
if __name__=="__main__":
    ap=argparse.ArgumentParser()
    ap.add_argument("--apply",action="store_true")
    ap.add_argument("--tests",action="store_true")
    a=ap.parse_args()
    target=TESTPATH if a.tests else PATH
    before=target.read_text(encoding="utf-8")
    after=(patch_test if a.tests else patch)(before,a.apply)
    if after!=before:target.write_text(after,encoding="utf-8")
    print("V242_FULL_ENVDIFFUSE_PHN_PAYLOAD_IDENTITY_GUARD_PASS")
