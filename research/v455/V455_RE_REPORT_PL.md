# V455 — native ChrIns descriptor gating, static RE

**Exact retail EXE SHA256** `a45aaa36dd2f6cc151670a639ea5547043cf38ea79ff4178b963c6ed71f98d7b`.
**Validation:** 45 exact opcode witnesses, 6 E8 edges, 45/45 negative mutations PASS in standalone offline handoff `DSRRL_V455_CHRINS_DESCRIPTOR_GATES_STATIC_RE.zip`. No game executable or addon modification. GitHub verifier below reproduces the essential gates with a SHA-locked PE reader.

## Verified gating
- `0x326B50`: `0x326B74` reads old descriptor `owner+0x370`, `0x326B7B` compares it with incoming `RDX`. When equal, `owner+0x4B1==0` skips update. Update path further requires `owner+0x384==0`; if descriptor nonnull, `descriptor+0x5A!=0`. `0x326BA9` saves incoming descriptor to `owner+0x370`. These are gating preconditions, not guaranteed LightBank writes.
- `0x326EF0`: requires `owner+0x370==nullptr`, `owner+0x384==0`; nonnull descriptor must have virtual type `2` via slot `+0x10`.
- `0x32D410`: selects descriptor using `owner+0x68`, flags `+0x2A5/+0x2A8/+0x2AA`; for type2 checks `owner+0x4B0==0` before calling `0x326EF0`. `0x32D4E3` sets `owner+0x4B0=1`. Alternative descriptor from `owner+0x4E0` routes to `0x326B50`.
- `model+0x93` writes: category5 path `0x326C0A→0x3E2120` and category1 `0x326F76→0x2F89B0` only write when resolver index nonnegative and different, setting dirty bit `0x20` in `model+0x84`.

**Interpretation:** a local PlayerIns descriptor switch is structurally possible, but not demonstrated in user's sunlight scene. Nearby shaded NPC row31 cross-contamination remains OPEN. Existing Source1→RX34 294/294 proves packet identity, not actor ownership or GPU pixel coverage. 27 CPU frame buckets with both rows are not proof of GPU overlap.

**Next:** narrow event-only capture at exact owner+0x370 and model+0x93 changes, with typed owner and lifecycle generation, then Seed/DrawEntity→RX34→pixel. No row forcing, no continuous CSV, no Source1 or protected R43 changes.
