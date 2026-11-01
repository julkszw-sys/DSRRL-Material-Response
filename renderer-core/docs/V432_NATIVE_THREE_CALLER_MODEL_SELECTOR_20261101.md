# V432 — Three original model-qualified selector callsites and alternate draw paths (2026-11-01)

## Purpose and source
Selector-only continuation of V431/V421, not shader or material RE. Based on owner-provided **unmodified** original DSR x64 PE SHA `a45aaa36dd2f6cc151670a639ea5547043cf38ea79ff4178b963c6ed71f98d7b`. Local verifier `verify_v432_three_caller_model_selector.py` certifies **47 aligned opcode preimages + 10 exact rel32 call targets**. It also supports verifying the addon hook source. R43 Source2, model cache policy, GPU and EXE are unchanged.

## New exact native model identity proof across three existing selector observer return addresses

All three original calls to `FrpgFlverModel::selector 0x22BA20` pass **exact model+0x88** (different original register assignments):
- Full draw `0x20DDA0`: `0x20DF00 lea rsi,[rdi+0x88]`, `0x20E011 mov rcx,rsi`, `0x20E014 call 0x22BA20`, return RVA `0x20E019`. Packet held in R15; `0x20DDFF mov r14,[r15]` yields FrpgDrawContext.
- Alternative `0x20E9D0`: `0x20EAD5 lea r15,[r13+0x88]`, `0x20EB4C mov rcx,r15`, `0x20EB7A call 0x22BA20`, return `0x20EB7F`. R13=model; R14=packet; `0x20E9FF mov rbp,[rdx]` yields FrpgDrawContext. Packet+4C/4E used at `0x20EB35/43`.
- Alternative `0x20F9F0`: `0x20FA2A mov rsi,rcx` (model), `0x20FAF5 lea r12,[rsi+0x88]`, `0x20FB6B mov rcx,r12`, `0x20FB99 call 0x22BA20`, return `0x20FB9E`. R15=packet; `0x20FA23 mov rbp,[rdx]` context. Packet+4C/4E used at `0x20FB58/62`.

Thus for **all three** already allowed selector return addresses, `FlverModel.this == container-0x88`. This did not justify V415 model decoding only for 20E019; it was a previously unnecessary diagnostic restriction. Both exact model class vtables `FgFlverModel RVA 12EB130`, `FlverModel RVA 12EB760` are already validated and preserved. It is **not** proof of actor, FLVER asset, or source A/B.

## Six FrpgModelDrawEntity builder branches — distinct downstream paths
Dispatch `0x220000`, constructor `0x21F900`, vtable `0x12ECA10`, from V431:
- Mask bit12 → `0x221A40`; no direct 20DDA0/20E9D0/20F9F0 call in this builder; uses distinct `0x1E36E0` downstream. No inference about nested draws.
- Mask bits30/31 → `0x2211C0` → **direct call 20E9D0** at `0x221412`.
- Mask bits0/4 → `0x220D60` → **direct call 20E9D0** at `0x22119B`.
- Mask bit18 → `0x221C10` → **direct call 20F9F0** at `0x222033`.
- Mask bits21/22 → `0x221450` → **direct call 20F130** at `0x2219F6`; this is not the same entry as 20F9F0. Code uses second `FrpgDrawParam` state `entity+0x120` in computation at `0x221898`; **not certified as packet+4C/4E for this path**.
- Default → `0x2200D0` → model virtual+18 `0x20DDA0`, packet A/B/beta first `entity+110/112/114`.

The five alternate builder output contracts are NOT simply the full builder with different flags; avoid applying one universal selector. There are **three model-qualified selector calls covered by the addon** and at least two alternative rendering paths requiring further census.

## Actual safe code change in V432 branch
Source file `renderer-core/src/runtime/flver_engine_hooks.cpp` — diagnostic only, within `DSRRL_V415_FLVER_MODEL_ID_DIAG`:
1. Widen the old `rva==k_ret_sel_1` typed model stamp gate to precisely `rva in {k_ret_sel_1,k_ret_sel_2,k_ret_sel_3}`. The enclosing selector observer already checked exactly those three returns.
2. Keep original `container-0x88`, typed vtable checks, reciprocal FgModel ancestry, generation witnesses, per-selector TLS clear, and observational journal. No change to Source1 cache keys or actor bridge eligibility.
3. Log actual return RVA in `[DSRRL V415 MODEL SELECTOR]` instead of hardcoded `caller=20e019`, enabling a diagnostic coverage census per native caller.

The changed file was independently re-fetched from GitHub and six static source-contract assertions PASS. This is a **source-only experimental diagnostic descendant**, not compiled or runtime-validated and not a new lighting bridge. Frozen V359-RC1 remains the working-base *release* and is untouched, as is R43 Source2.

## Remaining decisive join before authorized model→LightBank mapping
Need invocation-local `(builder ID, typed FrpgModelDrawEntity instance/generation, model.this, packet pointer while alive, packet[0] context, A/B/beta, packet E0/F0, model selector caller, Source1 native_producer_sequence/epoch, FLVER/MTD/slot, actual issued indexed draw and RTV/DSV)`. Validate A/B/beta *after* any same-packet prepass `0x20FEE0`; do not claim packer/selector cache match by timestamp, owner alone, or stale stack pointer. Source2 stays fail-open and independent. Do not patch shader/gamma/MTD or pin row30.

STATUS: original static model+0x88 3-callers CONFIRMED; V432 diagnostic code CONSTRUCTION (source only), build COMPATIBILITY OPEN, RUNTIME LIVENESS OPEN, model packet Source1 join OPEN, PIXEL OPEN.