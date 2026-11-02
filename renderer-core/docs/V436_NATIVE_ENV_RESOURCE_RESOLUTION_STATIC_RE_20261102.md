# V436 — Native GI/legacy Env resource resolver, not physical LightBank row

**Date:** 2026-11-02. **Source:** original SHA-locked retail DSR x64 PE `a45aaa36dd2f6cc151670a639ea5547043cf38ea79ff4178b963c6ed71f98d7b`. **Lineage:** branched directly from V435 HEAD `845bec4d28a85c370a5636c572cdb09d966f5ec3` (preserves all inherited V433 runtime code). No addon or EXE changes.

## Correcting V435 nomenclature

The previously identified native `0x1EADE0` and `0x1EB320` downstream consumers read packed selector A/B from the native model packet, but **their attested outputs are environment diffuse/specular resource handles, not proven LightBank PARAM rows or Source1 producer identities**. In particular, do not conflate these resource lookups with the independent `0x22BA20 -> 0x295F50` shader-set handle selection. V435 opcode/group+component facts survive; the broad label “physical LightBank selector” was misleading.

## Verified native graph

- **Legacy `0x1EADE0`:** calls `0x563DE0` for direct resource-record 32-bit IDs, or generates `EnvDif_%s_%03d` / `EnvSpc_%s_%03d` via `0x563E70` and `0x563F00`; names can include `_%1d` variants.
- **GI `0x1EB320`:** category-5 resource pointer per packed selector via `0x569B40(ctx+0x2320, group, 5)`; uses group-11/category-5 only as a null-result fallback. Formats `GI_EnvDif_%s_%02d_%04d` (optionally `_%1d`) using `0x563F90`, or `GI_EnvSpc_%s_%02d_%04d_%d` using `0x564000`. The formatters split the numeric component at 10000 (compiled `0x68DB8BAD` signed reciprocal and `0x2710` multiplier).
- **Name -> 32-bit handle `0x1EAD10`:** lookup of generated UTF-16 name in `FrpgDrawContext+0x2308` through `0x587150`, then `entry+0x28 -> object+0x10 DWORD`. Absent/zero ID takes a fallback lookup named `SYSTEX_INVALID`; it is not a bank-row-zero certificate.
- **Handle -> native binding `0xBADC0`:** changed handle in `context+0xD4+4*slot` causes cache invalidation at `+0x118+4*slot` and updates downstream resource state. The GI path calls `0x1EAC30` and `0xBADC0` in ordered slots 11/13 for EnvDiffuse A/B, 12/14 for EnvSpec A/B, with `ctx+0x2354` and `+0x2358` caching first handles. Same-selector branch can reuse A rather than format B a second time.

## Required next evidence

An **invocation-local** witness must associate the actual typed model, builder, packet A/B/E0/F0, context generation and resolved logical resource/handle with the independent Source1 native producer sequence and the *same issued RX34* PS/CB/RTV. Neither static pointer reuse nor a matching timestamp or packed word is proof. Probe assignment and LightBank color source must remain distinct operators until the shared consumer/routing evidence proves otherwise. Keep R43 Source2 `latest_hook_source()` unchanged; no new gate, row30 forced donor, asset replacement or pixel claims.

## Verification

The companion full V436 handoff archive includes `verify_v436_env_resource_resolution.py` (65 exact opcode checks, 37 rel32 calls, 8 RIP-relative name references, 10 UTF-16 literals), predecessor V434/V435 verifiers, negative fixtures, result JSON and SHA-256 manifest. The compact in-repository verifier has the same key source contracts. **Original PE static: CONFIRMED; Source1↔RX34: OPEN; addon build/runtime/bridge activation/PTDE pixels: OPEN.**
