# V456 — independent ChrModel selector channels (read-only RE)

Original DSR EXE SHA256 `a45aaa36dd2f6cc151670a639ea5547043cf38ea79ff4178b963c6ed71f98d7b`. Offline validation 23 exact opcode witnesses, 8 direct CALL edges, 24/24 negative mutations PASS. Source-complete standalone verifier and full results are in the accompanying V456 ZIP handoff.

**New native structure:** First descriptor family resolves and conditionally writes three separate signed-byte indices on model:
- `0x326C0A→0x3E2120` writes `model+0x93`, dirty `model+0x84 |= 0x20` at `0x326C22/29`.
- `0x326C32→0x3E21C0` writes `model+0x94`, dirty `0x40` at `0x326C4A/51`.
- `0x326C5A→0x3E2260` writes `model+0x95`, dirty `0x80` at `0x326C72/7C`.
- `0x326C86` loads 16-bit `descriptor+0x48`; `0x326C93/9D` conditionally writes `model+0x96`, dirty `0x100`.

Alternative descriptor family has separate resolvers:
- `0x326F76→0x2F89B0` writes `model+0x93`, dirty `0x20` at `0x326F8E/95`.
- `0x326F9E→0x2F8A50` writes `model+0x94`, dirty `0x40` at `0x326FB6/BD`.
- `0x326FC6→0x2F8AF0` writes `model+0x95`, dirty `0x80` at `0x326FDE/E8`.

Every index write is gated by a nonnegative resolver result and difference from previous value. A local owner switch can therefore alter multiple independent selector channels without changing `model+0x93`. **Do not assume all channels are LightBank or all reach RX34:** downstream consumer census and gameplay actor identity are OPEN. No evidence that row31 belongs to shaded NPC, changes player armor pixels, or is incorrect. No addon/EXE/Source1/R43 changes; no blanket row30 override. Status STATIC PASS, runtime/pixel OPEN.
