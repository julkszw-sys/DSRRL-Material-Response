# V416 — Reciprocal FgModel ↔ FgFlverModel identity RE
2026-10-29 · Renderer Edition · single-addon **diagnostic** descendant

## Provenance

* Original DSR PE SHA256: `a45aaa36dd2f6cc151670a639ea5547043cf38ea79ff4178b963c6ed71f98d7b`.
* Pinned parent V415 source: `39dc73308e0ed371f1cf9a17c892f07a2298104c`. Frozen V359-RC1 remains unchanged.
* The original user V413 runtime log is **not** V416 runtime evidence. No new in-game V416 test as part of this static RE.

## Confirmed original-exe ABI

RTTI CompleteObjectLocator (MSVC x64; RVA with base 0x140000000) identifies:
* `NS_FRPG::FgModel` vtable RVA `0x13DF6D0`.
* `NS_FRPG::FgFlverModel` vtable RVA `0x12EB130`.
* `NS_FRPG::FlverModel` vtable RVA `0x12EB760`.

The FgModel ctor RVA `0x76E260` installs its vtable at `0x76E277..0x76E27E`, initializes `parent+0x28` to null, saves constructor argument RDX to `parent+0x30`, and sets refcount `parent+0x40=1`. That upstream RDX pointer is **unclassified**, NOT necessarily a character/actor ID. At RVA `0x76E3D2`, ctor passes `R9=FgModel this` to `FgFlverModel` ctor RVA `0x20C6E0`; at `0x76E3E3`, stores returned child in `FgModel+0x28`. Child constructor `0x20C726` saves that parent as `FgFlverModel+0x140`. Alternative child constructor RVA `0x20C770` explicitly sets `+0x140=0` and is not parent-admitted. FgModel destructor RVA `0x76E430` reads `parent+0x28` at `0x76E484`, calls virtual release and clears pointer at `0x76E4B3`.

Therefore the **two-sided pointer and exact RTTI** can validate a model ownership relationship during a draw, independent of the shared `FrpgDrawContext`; they do **not** establish gameplay actor identity or lifetime generation. `FgModel` itself is instantiated from different native callsites with different constructor flags and upstream source objects, so assuming a universal actor classification is unsupported.

Native typed selector path from V415 is preserved unchanged: model draw RVA `0x20DDA0`; `FlverModel+0x88` is material selector container at callsite `0x20E014`, returning to `0x20E019`; pass context is read from the draw packet, not inferred from the model pointer.

## V416 code

At verified selector callsite only, **read-only**:
1. V415 authenticates a typed `FlverModel*` using class vtable.
2. V416 for *only* FgFlverModel reads `model+0x140`; checks readability, parent vtable `0x13DF6D0`, and reciprocal `parent+0x28 == model`; records `parent+0x30` as opaque `upstream_hint` (NOT used as actor identity).
3. Per-thread witness is reset for each selector opportunity; verified parent goes into TLS only. `[DSRRL V416 FGMODEL PARENT]` emits bounded samples and reason code: 0 non-Fg, 1 no parent, 2 unreadable, 3 wrong RTTI, 4 wrong backlink, 5 reciprocal match.
4. `[DSRRL V416 FGMODEL SOURCE JOIN]` logs parent, model, context, material index and native Source1 producer sequence/row only when the bidirectional witness matches. No Source1 admission, LightBank, shader, GPU CB, RTV, or draw replay changes.

The protected R43 `latest_hook_source()` and independent shader/material/resource islands remain untouched. No arbitrary bright/dark row pinning. A stale or unrecognized model leaves V416 diagnostic witness absent (stock renderer policies unchanged).

## Interpretation of a future V416 in-game log

* If row30 and row31 for the SAME RX34/FLVER/MTD/source are associated with **different valid FgModel parent pointers**, there is strong evidence that an apparent `FrpgDrawContext` key aliases separate renderable owners. Still need actual GPU pass/RTV + draw-CB and visibility to prove the dark armor pixel came from the wrong owner.
* If they share the SAME parent and child, source row variation is **within a single native model object**, so look for render-pass, material submesh, temporal selection or GPU CB/RTV mismatch before calling it an actor mixup.
* Null/unreadable/invalid parent, non-Fg model or missing bounded sample is **OPEN**, never evidence that two unknown objects are equal.

## Test levels

* STATIC ABI: confirmed from original EXE disassembly and RTTI; 19 byte-for-byte instruction preimages and 3 class RTTI tested using `test_v416_fgmodel_parent_exe_abi.py` against SHA-pinned original EXE.
* CONSTRUCTION / COMPATIBILITY: determined independently by Windows CI; never promote to runtime or pixel.
* RUNTIME LIVENESS, BRIDGE ACTIVATION, PTDE PIXEL: **OPEN** without V416 gameplay log and matched reference image.

Next semantic cut after the two-sided parent test: trace exact model + context + producer sequence through issued RX34 draw, CB and render target to the presented pixel, including class/lifetime generation and pass identity.