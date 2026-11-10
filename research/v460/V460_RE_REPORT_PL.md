# V460 — message registry handle versus Seed resource key

Original, unmodified retail EXE SHA256 `a45aaa36dd2f6cc151670a639ea5547043cf38ea79ff4178b963c6ed71f98d7b`. Read-only static audit: **56 exact instruction preimages**, **11 direct E8 edges**, **63/63 negative mutations PASS**. No addon build or gameplay runtime claimed.

## Verified registration split

`FrpgModel+0x18` is passed to `0x157CF0` from `0x21C554` (second at `0x21C71F`). Inside `0x157CF0`, **R15D** comes from independent `0x1D2180` (`[global+0x10]`) and becomes `FrpgModelDrawEntitySeed+0x10` through `0x157DDD→0x157DF6→0x15A97E`. Conversely **EDI** comes from allocator `0x11BA160` called at `0x157D24`, is returned at `0x157E00` and stored in **`FrpgModel+0x10`** at `0x21C559/0x21C724`. They come from distinct producers. Never equate them without evidence; this does not assert their values are always unequal.

Allocator `0x11BA160` combines a low 24-bit slot index, high-byte slot metadata and sets bit31 (instructions `0x11BA1B0–0x11BA1D2`). Neither the 32-bit handle nor `Seed+0x10` is automatically a gameplay actor ID or pointer.

## Opcode 0x804C is addressed by FrpgModel pool handle

`0x21CAA1` reads `FrpgModel+0x10`, `0x21CAC7→0x222AC0` sends three Light/Fog/Scattering packed keys; `0x222ACA` passes that model handle in R8D and `0x222AD2` sets opcode **0x804C**. Header helper `0x11B3710` writes the model pool handle to `message+0x10` and opcode to `+0x14`.

**Stateful delta encoding:** `0x11B3710` checks whether recipient handle changed. If it is unchanged, bit16 of opcode is set (`0x11B371D`). Serializer `0x11B36B0` tests bit16 (`0x11B36E7`) and, when set, **skips serializing the recipient handle**; otherwise it writes `message+0x10` into the outgoing stream (`0x11B36ED/F0`). A raw per-message capture can therefore lack its target: correct reconstruction needs prior routing context and decoder state. Previous V457/V458 verified a typed receiver at `FrpgModelDrawEntity`, but not this dispatcher’s handle→receiver owner association.

## Independent Seed resource side

`0x1F2DE6` reads `Seed+0x10`, calls `0x11B3D50`, stores resource pointer at `FrpgDrawEntity+0x20`. `0x21F93E/F942` copies `Seed+0x20` into `FrpgModelDrawEntity+0x40`; `entity+0x50` is initially zeroed. This resource path is distinct from message recipient handle. The previously observed common engine FLVER resource for six RX34 entities is NOT actor identity.

## Remaining open edges

1. Exact runtime **registered handle → stateful stream decoding → concrete FrpgModelDrawEntity** (including lifetime/reuse).
2. **ChrIns/PlayerIns/EnemyIns → ChrModel* → model registration handle** for each of the six entities and equipment parts.
3. Their **receiver-specific shader/CB and visible pixel coverage**, then PTDE comparison.

V459 already falsified in-entity Light 30↔31 lerp for the captured Source1 case. Repeating it does not resolve these edges. No patch and no user runtime requested. The newest narrow candidate test is a single **registration/dispatch owner-identity certificate**, not a new Light-pair capture. Remain fail-open and preserve Source1/Source2/R43 and all unrelated renderer subsystems.

Offline self-contained full report and verifier are also bundled in `DSRRL_V460_MESSAGE_REGISTRY_HANDLE_RE_FULL.zip` SHA256 `c51c3e52a1ff5e4b180f6c7e5af82dbadf92b86e50571c33d395ea9de70c6e44`.