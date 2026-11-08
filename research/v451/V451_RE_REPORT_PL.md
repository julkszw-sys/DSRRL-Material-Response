# V451: native model-resource identity at RX34 (offline RE + diagnostic bridge)

**Target:** PTDE visible lighting; DSR host. **Status:** STATIC PASS only; V451 Windows build/runtime and PTDE pixel must be checked independently.

## Source / prior falsifiers
- Original `DarkSoulsRemastered.exe` SHA256: `a45aaa36dd2f6cc151670a639ea5547043cf38ea79ff4178b963c6ed71f98d7b`.
- V448 CSV: 294/294 independently certified Source1 RX34 packets, 12 row30 Source2, and six different native model-draw entities partitioned 2 row30/4 row31. Identical engine FLVER or FrpgDrawContext does not imply gameplay actor identity.
- V450 already disabled automatic 180-second CSV and expensive failed 4558 selector-stack probes.

## New exact static graph
1. Native `FrpgModelDrawEntity` factory `0x223AC0` passes fourth argument `r9` into constructor `0x21F900`. MSVC RTTI identifies its parent type **`FrpgModelDrawEntitySeed`**: vtable `0x12CC308` resolves the same virtual factory `0x223AC0`; the ASM counterpart is `FrpgAsmModelDrawEntitySeed`, vtable `0x12CC2E8`. It is NOT the same type as `FrpgModel`.
2. Constructor `0x21F93E` reads descriptor `+0x20` and stores a model pointer in `drawEntity+0x40` at `0x21F942`.
3. Base constructor `0x1F2DB0` reads descriptor `+0x10` at `0x1F2DE6`; `0x1F2DEC` calls resolver `0x11B3D50`; `0x1F2DF1` stores resulting model-resource pointer in `drawEntity+0x20`.
4. Resolver `0x11B3D50` masks an ID with `0x00FFFFFF` for nonnegative IDs, then accesses a resource registry via `+0x30`, pointer array `+0x08`, and indexed `qword[base + 8*id]`. Negative IDs use a sentinel; never invent a valid pointer from it.
5. The live builder `0x2200D0` reads `drawEntity+0x20` at `0x220198` and obtains `float[resource+0x58]` at `0x2201AE`; the instance resource is a native input actually consumed during draw generation.
6. Separately, the typed `FrpgModel` constructor `0x21C3B0` stores `+0x18` parent reference, registers its own `+0x10` ID at `0x21C554 -> 0x157CF0`, and `0x21C970` emits event `0x804C` using that ID. **Do not equate** this `FrpgModel` ID or its `+0x20` flags with the untyped descriptor of item 1. Without an original pointer/RTTI join, `FrpgModel -> FrpgModelDrawEntity` is OPEN.

Static verifier `research/v451/verify_v451_native_model_resource.py`: **28** exact instruction witnesses, **4** direct call edges, **4** RTTI types and **37/37** negative mutations; original EXE never modified.

## V451 scope
V451 adds one read-only event kind21 to the existing **manual F9 / 8-second, cap1024, RX34 route345 row30/31** capture:
`(native sequence, source-kind, row, FrpgModelDrawEntity, FrpgDrawParam, engine model, resource pointer entity+0x20, optional entity+0x50, packet, builder-frame, validity, slot, route, FrpgDrawContext)`.

- No additional EXE detour, no new selector unwind, no automatic CSV, no row override, no resource or shader patch.
- If native issued builder was not independently certified, pointers are not dereferenced; record validity flags and zeros.
- `entity+0x20` is a native model-resource pointer, **not gameplay actor ID**. Grouping is a diagnostic; identical addresses across models, instance reuse and allocation lifetime are unresolved. `entity+0x50` is opaque optional state.
- Exact actor→pixel/RT coverage and PTDE visual equivalence remain OPEN.

## Intended falsification
Run V451 in same armor scene with normal telemetry inactive; press F9 once, briefly rotate camera and optionally F10 marker, submit the **small** `%TEMP%\\DSRRL_V438\\V438_*.csv` and ReShade log. Execute `python renderer-core/tools/analyze_v451_model_resource_capture.py capture.csv`. Compare resource-pointer/grouping by row/entity/slot. Do not promote to gameplay actor identity or activate bridge based on this alone.
