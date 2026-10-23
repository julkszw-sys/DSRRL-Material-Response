#!/usr/bin/env python3
"""Exact, fail-open Warrior Standard Helm resource route candidate.

Source basis: owner packed PTDE FLVER2+TPF+DDS SHA census, HD_M_9360 slot0,
C_Metal[DSB] route229, RX33-35 only, live DSR owner/slot/MTD must authenticate.
This is an operator-local test of t0/t1/t2 and never a family-wide alias.
"""
from pathlib import Path

PATH = Path("renderer-core/src/runtime/material_resource_draw_runtime.cpp")
text = PATH.read_text(encoding="utf-8")

def once(before, after):
    global text
    n = text.count(before)
    if n != 1:
        raise SystemExit(f"Source drift expected 1 anchor, got {n}: {before[:90]!r}")
    text = text.replace(before, after, 1)

once(
"""    const auto h2 = hashes[2];

    if (full_material_response_ready &&""",
"""    const auto h2 = hashes[2];

#if defined(DSRRL_HD_M_9360_EXACT_TRIPLE)
    // PTDE source: HD_M_9360 base FLVER, material slot0, raw C_Metal[DSB],
    // exact HD_M_9360 / _s / _n logical DDS. This is NOT a P_Metal island.
    // It cannot match a different material, shader family or wrong DSR t0-2.
    // The generic route name already exists (229); it was the V12 resource
    // pair/triple allowlist that lacked this *male* helm.
    const bool exact_hd_m_9360_resource_triple =
        receiver_id >= 33u && receiver_id <= 35u &&
        query.material.valid &&
        query.material.owner_tuple_exact &&
        query.material.material_slot_valid &&
        query.material.material_slot == 0u &&
        query.material.route_index == 229u &&
        query.material.semantic_name_hash ==
            operators::material_response::mtd_semantic_hash(
                "C_Metal[DSB].mtd") &&
        operators::legacy_plan::hashing::matches_hex(
            query.material.raw_mtd_sha256,
            "ae2e8df867fe2859eef37104c13c939e7e7fe4f703b7fe49c408d0230fc71d85") &&
        operators::material_response::has_exact_flver_material_ownership(
            query) &&
        h0 == 0xcc1735c736ad61fcull &&
        h1 == 0xe9c532be429d6f7eull &&
        h2 == 0xe9c53fbe429d8595ull;
#else
    constexpr bool exact_hd_m_9360_resource_triple = false;
#endif

    if (full_material_response_ready &&""")
once(
"""        generated::diffuse_pair_allowed_v12(
            h1,
            h0);

    if (bmp_receiver &&""",
"""        (generated::diffuse_pair_allowed_v12(h1,h0) ||
         exact_hd_m_9360_resource_triple);

    if (bmp_receiver &&""")
once(
"""        generated::normal_tuple_allowed_v12(
            h0,
            h1,
            h2);

    if (bmp_receiver &&""",
"""        (generated::normal_tuple_allowed_v12(h0,h1,h2) ||
         exact_hd_m_9360_resource_triple);

    if (bmp_receiver &&""")
# Resource cache loader also uses target v12 allowlist, BEFORE the draw path.
# Without this exact name-scoped preload the added draw route is unreachable.
once(
"""                      ? generated::diffuse_target_hash_allowed_v12(hash)
                      : generated::normal_target_hash_allowed_v12(hash);""",
"""                      ? (generated::diffuse_target_hash_allowed_v12(hash)
#if defined(DSRRL_HD_M_9360_EXACT_TRIPLE)
                         || hash == 0xcc1735c736ad61fcull
#endif
                        )
                      : (generated::normal_target_hash_allowed_v12(hash)
#if defined(DSRRL_HD_M_9360_EXACT_TRIPLE)
                         || hash == 0xe9c53fbe429d8595ull
#endif
                        );""")
PATH.write_text(text,encoding="utf-8")
assert "exact_hd_m_9360_resource_triple" in text
print("HD_M_9360 exact material/DSR ownership/RX/t0-t2 scoped route materialized")
