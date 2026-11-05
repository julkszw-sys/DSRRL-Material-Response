# V440-R3 exact alternative builder type5 producer and model packet

**Owner original:** retail DSR exe sha256 `a45aaa36dd2f6cc151670a639ea5547043cf38ea79ff4178b963c6ed71f98d7b`. V439 addon ancestor preserved; no V440 addon or executable patch. **Status:** STATIC CONFIRMED. Dynamic model type, game actor generation, exact issued RX34 and PTDE visible output OPEN.

## Root

The alternative builder `RVA 0x2162D0..0x217363`, exact `.pdata=(0x2162D0,0x217363,0x174D464)` with UNWIND flags3 (not a child of default builder 0x2200D0). Entry `0x2162D0 mov rax,rsp`; `0x2162E2 lea rbp,[rax-0x268]`; `0x21631A mov rdi,rcx` actor, stack packet at `RBP+0x20` = builder entry RSP-0x248.

## Matching producer/packet

- `0x216414/1E` actor+0x110 getter A; `0x21642D` writes packet `RBP+0x6C` = packet+0x4C.
- `0x216423/36` actor+0x110 getter B; `0x21647D` writes packet `RBP+0x6E` = packet+0x4E.
- `0x21643B/4E` actor+0x114 beta into packet `RBP+0x70` = packet+0x50.
- `0x2167AD` passes `R8=actor+0x108`, `0x2167BA` calls original type5 wrapper `0x1C0BE0`; producer return `0x2167BF` in same unwind root.
- `0x216C13` calls `0x20FEE0` optional prepass with `RDX=RBP+0x20` on its path.
- `0x21729C` writes `packet+0x08` per selected submodel; `0x2172A3` loads model `RCX=[RBX+RSI]`, `0x2172AA` indirect `call [model.vtable+0x18]` with `RDX=RBP+0x20`; continuation `0x2172AD` in SAME unwind root. Actual FgFlverModel/FlverModel vtable type of each submodel remains conditional.

Unlike default builder, native type5 is unconditional on this source path. Model subparts can update packet+0x08 independently; do not pretend entire packet is one immutable material. The original prepass and asset identity still need transitive/live validation.

This proves a second **same builder-frame source→packet→virtual-model call** graph, **not** exact source→issued RX34, no proven actor age/generation or pixel equivalence. Proven original non-helper vtable slot destinations were identified in earlier V414; this path must validate dynamic vtable before claiming the callee `0x20DDA0`.

## Diagnostic consequences

Packer type5 calls have two direct native enclosing builder callsites: default `0x220639` and alternative `0x2167BA`. Exact packet identity is path-specific: default `RBP-0x30`; alternate `RBP+0x20`. A fixed offset unwinder built only for default would silently misidentify alternative draw packet. Fail-open, never join by 'last producer' or same source-row.

Reproducible static witness in `renderer-core/tools/verify_v440_alternate_builder.py`: SHA-locked original exe, 21 exact opcodes, 4 direct CALL edges, original parent pdata root, 5 runtime call sites. No EXE distribution or runtime activation.
