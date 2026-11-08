# V453 — targeted native LightBank writer and registration callsite census

## Original EXE / scope
Read-only static audit of original `DarkSoulsRemastered.exe` SHA256 `a45aaa36dd2f6cc151670a639ea5547043cf38ea79ff4178b963c6ed71f98d7b`. The attached source `verify_v453_writer_callsite_census.py` checks opcode preimages and all raw relative E8 candidate sites in executable PE sections. A raw byte-level callsite census is **not** full control-flow recovery; embedded bytes can resemble E8, while indirect calls/tailcalls/obfuscated dispatch can be missed.

## New evidence beyond V452
1. **A second direct registration site** exists at RVA `0x21C71F` in addition to `0x21C554`. At `0x21C71B`, `RDX ← [RSI+0x18]` (model reference); `0x21C71F` calls registration `0x157CF0`; `0x21C724` writes return `EAX` to `[RSI+0x10]`. This means the same `FrpgModel` registration handle/Seed creation channel is reachable from at least two native callsites. The second path must be included in any future lifecycle hook/census; constructor-only capture could miss it.
2. Exact direct `E8` candidates for family writer `0x326320`: `0x326C02`, `0x326F6E`, `0x3270B1`. These are the three V452 observed descriptor-to-model family update branches, **not evidence of NPC ownership**.
3. The index writer `0x326360` has **zero direct raw E8 candidates** in executable sections. It is still a valid independently verified native writer, and its lack of direct E8 references does **not** establish that it is unused: other paths write `model+0x93` inline, and indirect/obfuscated routes are possible.
4. `PlayerIns` virtual slot `0x355890` has one raw direct E8 candidate `0x36639D`, whose context then calls a separate virtual method. The function remains obfuscated. This does **not** certify `PlayerIns+0x60` or any owner-to-RX34 mapping.

## Impact on the player's sunlight / NPC shade hypothesis
Two distinct registered model-draw entities can share FLVER and material slot while retaining separate native LightBank source values. The 294/294 Source1 producer→issued matches do not establish that a row31 draw belongs to the player, that it affects the player's pixels, or that it came from the nearby NPC. In particular, the second model registration callsite means **a lifecycle witness attached only to `0x21C554` is incomplete**.

## Next narrow, low-overhead test
Instead of continuous CSV or per-draw stack unwinding, capture a **bounded one-shot** registration/lifetime identity table at the two `0x157CF0` callsites and actor-specific owner model allocation, then correlate only the six RX34 draw entities at the native issue boundary. If the exact typed actor pointer+generation is not available, fail-open. No row30/31 override. The V451 manual F9 cap remains unchanged; no V453 addon is built.

**Status:** static routing census only. Runtime gameplay actor, actual GPU overlap, PTDE-visible pixels OPEN.
