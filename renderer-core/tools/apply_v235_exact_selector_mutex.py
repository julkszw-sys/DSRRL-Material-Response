#!/usr/bin/env python3
"""V235 opt-in: exact-selector-only cache contention normalization.

Producer behavior (try_lock / full donor decode) remains unchanged.
Selector consumer waits only for the *same per-set cache mutex* before
checking the exact R43 key. No receiver, material, source-key, payload,
resource identity, shader or pixel math changes.

This is an isolated AB experiment: removes false misses caused solely by
mutex busy, while an actual empty/wrong-key cache still fails open to the
original full donor decode. No cap, eviction, VRAM budget, or global lock.

Call with --apply against V233's generated native C++ overlay before build.
"""
from __future__ import annotations
import argparse
import pathlib
import sys

PATH = pathlib.Path("renderer-core/src/runtime/pmetal_env_source_runtime.cpp")
OLD_SIGNATURE = """    const pmetal_envspec_source *payload = nullptr) noexcept
{
    out = {};
    if (!key.valid)
        return false;

    const auto set =
        hook_producer_cache_set(key);
    std::unique_lock<std::mutex> lock(
        g_hook_producer_cache_mutex[set],
        std::try_to_lock);
    if (!lock.owns_lock()) {
        g_hook_producer_cache_busy.fetch_add(
            1u,
            std::memory_order_relaxed);
        return false;
    }
"""
NEW_SIGNATURE = """    const pmetal_envspec_source *payload = nullptr,
    bool wait_for_exact_selector = false) noexcept
{
    out = {};
    if (!key.valid)
        return false;

    const auto set =
        hook_producer_cache_set(key);
    // V235: exact draw consumers wait for the tiny keyed cache critical
    // section rather than mistaking a busy mutex for a missing payload.
    // Producer lookups retain R43 try_lock, with full-donor fallback.
    std::unique_lock<std::mutex> lock(
        g_hook_producer_cache_mutex[set],
        std::defer_lock);
    if (wait_for_exact_selector) {
        lock.lock();
    } else if (!lock.try_lock()) {
        g_hook_producer_cache_busy.fetch_add(
            1u,
            std::memory_order_relaxed);
        return false;
    }
"""
OLD_CONSUMER = """    if (!hook_producer_cache_lookup(
            query,
            cached))
        return false;
"""
NEW_CONSUMER = """    if (!hook_producer_cache_lookup(
            query,
            cached,
            nullptr,
            true))
        return false;
"""
def audit(text: str, apply: bool) -> str:
    is_new = NEW_SIGNATURE in text and NEW_CONSUMER in text
    is_old = OLD_SIGNATURE in text and OLD_CONSUMER in text
    if is_old == is_new:
        raise ValueError("V235 source anchor mismatch: must be exactly one of stock/V235")
    if text.count("bool hook_producer_cache_lookup(") != 1:
        raise ValueError("V235 producer helper signature no longer unique")
    if text.count("void hook_producer_cache_publish_exact(") != 1:
        raise ValueError("V235 cache writer provenance no longer unique")
    if not all(s in text for s in (
        "same_hook_selector_identity(", "same_hook_source_payload(",
        "g_hook_producer_cache_generation.load(", "g_hook_producer_cache_mutex[set]",
        "g_hook_source_mutex", "if (hook_producer_cache_lookup(\n            selector_identity,",
        "void clear_hook_source() noexcept")):
        raise ValueError("V235 missing protected source semantics")
    if not apply:
        print("V235_EXACT_SELECTOR_MUTEX_ABLATION_PASS=" + ("PATCHED" if is_new else "STOCK"))
        return text
    if is_new:
        print("V235_EXACT_SELECTOR_MUTEX_ABLATION_IDEMPOTENT")
        return text
    patched = text.replace(OLD_SIGNATURE, NEW_SIGNATURE).replace(OLD_CONSUMER, NEW_CONSUMER)
    if patched.count("wait_for_exact_selector") != 2:
        raise ValueError("V235 unexpected selector routing count")
    print("V235_EXACT_SELECTOR_MUTEX_ABLATION_APPLIED producer_trylock=ON selector_wait=ON")
    return patched

def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--apply", action="store_true")
    ap.add_argument("--path", type=pathlib.Path, default=PATH)
    ns = ap.parse_args()
    text = ns.path.read_text(encoding="utf-8")
    modified = audit(text,ns.apply)
    if ns.apply and modified != text:
        ns.path.write_text(modified,encoding="utf-8")
if __name__ == "__main__":
    main()
