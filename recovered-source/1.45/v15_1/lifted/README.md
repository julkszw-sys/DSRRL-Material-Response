# V15.1 recovered lifted source

Status: **BYTE-EXACT LIFTED SOURCE / NOT ORIGINAL C**

This source removes the opaque `v15_1_ext_text.bin` and `v15_1_ext_rdata.bin` prerequisites from the recovery build.
The historical extension is represented as:

- `envcube_v15_1_lifted.S`: exact machine-code bytes split under named ABI/function entry labels;
- `envcube_v15_1_rdata.py`: declarative 368-route EnvSpcSlot map + 342 semantic probe records + exact small descriptor/string/jump-table tail;
- `V151_RECOVERED_ABI.json`: known entrypoints, host callsites, state-offset surface and immutable hashes;
- `build_v15_1_from_lifted_source.py`: rebuilds exact V15.1 from exact V12 + textual lifted source + PackedGI.

The generated addon must be SHA-256 `7f10f908d598f8d6b952e2c78bce6eb97a8b84a297f956a069c439b7977b65cc`.

## Named code surface

- `envcube_formatter_detour` — formatter A/B semantic capture
- `envcube_prepare_wrapper` — inherited prepare + per-thread receiver staging
- `envcube_pre_wrapper` — route/resource/identity/transaction gate and t12/t14 bind
- `envcube_restore_transaction` — stock t12/t14 restore + COM release
- `envcube_ensure_cache` — PTDE TextureCube/SRV realization from PackedGI
- `envcube_gpu_identity_observe` — two-hit GPU identity establishment/refresh/conflict fail-open
- `envcube_txn_find_or_alloc` — context transaction table helper
- `envcube_post_wrapper` — restore then inherited post
- `envcube_init_wrapper` / `envcube_uninit_wrapper` — hook lifecycle and cleanup

## Rdata recovered structurally

The first 368 bytes of V15.1 rdata are exactly the route `EnvSpcSlotNo` sequence from `V12_ROUTE_ENVSPC_SLOT_MAP_COMPLETE.json` (195 slot0, 59 slot1, 62 slot2, 52 slot3).
Immediately after it are 342 records of three little-endian u32 values used by the formatter semantic probe lookup.
The remaining 3712-byte tail holds resource descriptor constants, diagnostic strings and compiler jump tables; it is retained exactly but is not yet fully semantically decomposed.

## Limitation

This is substantially more useful than binary payload blobs and is exactly rebuildable, but it is **not the lost handwritten V15.1 C source**. The project source-complete gate therefore remains closed. Future clean development should replace lifted byte functions one-by-one with source implementations while preserving ABI/tests, rather than editing the provenance bytes silently.