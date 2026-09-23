# V15.1 EnvSpec semantic map

Verified historical RVAs from the V15.1 static audit and binary disassembly:

- `0x1B7000` — `envcube_formatter_detour`
- `0x1B73D0` — module/base helper used by all wrappers
- `0x1B73F0` — `envcube_prepare_wrapper`
- `0x1B74C0` — `envcube_pre_wrapper`
- `0x1B80E0` — shared transaction/state helper (called by PRE, POST and unload path)
- `0x1B8250` — resource/cache helper used by PRE
- `0x1B8860` — helper used by EnvSpec GPU identity/resource path
- `0x1B8A90` — helper used by transaction/resource setup
- `0x1B8E10` — `envcube_post_wrapper`
- `0x1B8E50` — `envcube_init_wrapper`
- `0x1B91F0` — `envcube_uninit_wrapper`

## Verified operator contract

The extension is a RESOURCE-layer EnvSpec bridge for ordinary DifSpcBmp local receiver ordinals `0..22` only. It does not replace the stock DSR receiver equation, sampler or shader body.

Its route is:

`CPU formatter semantic A/B -> per-thread semantic store -> stock SRV identity learning/confirmation -> exact material EnvSpcSlotNo -> PTDE PackedGI SRV cache -> draw PRE save stock t12/t14 -> bind exact PTDE t12/t14 -> draw -> POST exact restore/release`.

Fail-open cases include unknown receiver, missing route, missing A/B endpoint, null stock resources, ambiguous or refreshing GPU identity, missing PTDE cache, and missing transaction state.

GPU identity requires two consistent fresh semantic observations before establishment. Learning/refresh draws fail open. Cached draws resolve only through unique established stock-SRV identity. `A == B` is permitted only when stock `t12 == t14`.

## Historical defect and safe successor

V15.1 stored extension state in V12-owned `.v13d` range `0x112300..0x112316`, causing the startup crash. V15.5/V15.6 relocate all extension references to `0x112260..0x112276`; V15.7 then reactivates the exact V15.1 PRE/PREPARE/POST transaction wrappers. The resource/operator logic is otherwise the same lineage.