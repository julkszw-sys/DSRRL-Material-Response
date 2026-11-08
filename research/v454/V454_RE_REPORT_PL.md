# V454 — shared ChrIns model interface, direct Seed constructors, descriptor-router RE
**STATUS: STATIC CONFIRMED, actor-to-RX34 OPEN, PTDE pixel OPEN.**

Original retail DSR EXE SHA256: `a45aaa36dd2f6cc151670a639ea5547043cf38ea79ff4178b963c6ed71f98d7b`. This is read-only evidence, not a game-runtime witness. No EXE/mod/shader/LightBank mutation.

## Discovery 1 — model field shared across PlayerIns and EnemyIns
Exact RTTI/vtable census: `ChrIns@0x131E008`, `EnemyIns@0x1322E68`, `PlayerIns@0x13251F0` have **the identical vtable slot +0x120 → 0x321CE0**. At RVA `0x321D05`, this inherited implementation reads `this+0x60`, null-checks the model pointer, and at `0x321D28` invokes the model's virtual slot `+0x28`. `ChrModel` vtable `0x12EAF30` supplies that slot as `0x20BA20`. The EnemyIns constructor path explicitly creates ChrModel `0x34A173 → 0x20B0B0` and stores it to `EnemyIns+0x60` at `0x34A17F`. PlayerIns also calls shared ChrIns base ctor at `0x354BD4 → 0x31E0C0`, and shares the model-interface method. **Its exact ChrModel allocation/store is still unverified**, since PlayerIns virtual model initialization `0x355890` is obfuscated. The only direct E8 call to ChrModel ctor is presently `0x34A173`; absence of another direct call does not prove absence of a PlayerIns model.

## Discovery 2 — two additional direct typed Seed constructions
An executable-sections CALL census to `FrpgModelDrawEntitySeed` ctor `0x15A8E0` finds precisely `0x157DF6` (known registration `0x157CF0`), `0x4FBF21` and `0x4FDAD1`. Both newly identified paths pass:
- `EDX = [RDI]` — source-local ID, without established gameplay semantics;
- `R8 = [[RDI+0x8]+0x28]` — model/resource reference;
- `RCX = &[RBP+0x17]` — temporary local Seed object.

They **do not call the known registration function `0x157CF0` at that site**. This falsifies completeness of an observation system attached only to the two FrpgModel→registration callsites from V453. The downstream object family/receiver for both paths is still unknown; they must not be presumed to belong to NPCs or the player.

## Discovery 3 — descriptor router inside the same ChrIns owner
At `0x32D448` the native method saves incoming `ChrIns` owner as `RSI`. Dependent on the upstream descriptor state, it calls:
- `0x32D4BA → 0x326B50`, `RCX = RSI`, descriptor from `owner+0x68` path;
- `0x32D4DE → 0x326EF0`, **same owner pointer**, distinct descriptor-kind branch;
- `0x32D518 → 0x326B50`, **same owner pointer**, fallback descriptor from `owner+0x4E0`.

V452 separately proved these writer families can change the model selector `FrpgModel+0x93` via owner`+0x60`. Thus an **intra-actor descriptor switch** is a concrete competing mechanism for row30/row31. This is **not** a runtime claim that it occurs in the user's sunlight/NPC scene.

## Hypotheses still open
- H_NPC: nearby shaded NPC's row31 somehow contaminates the visible player's model/pixels.
- H_SELF: a descriptor transition within PlayerIns changes its own LightBank before Source1.
- H_RENDER: two legitimate native entities create unintended overlapping, culling, or pass effects.

Earlier 294/294 Source1 native source→issued RX34 matches only certify the native packet fields, **not gameplay actor identity or pixel overlap**. Common FLVER/model resource or FrpgDrawContext is not a valid actor key.

## Next test, smallest justified scope
No broad CSV, no per-draw stack walking. If a runtime observer is introduced, capture an **event only when descriptor route/selector changes**, including verified PlayerIns/EnemyIns vtable, owner pointer, `owner+0x60` typed model, `model+0x93` before/after, descriptor source/ID, and true creation/destruction generation. Recover the typed Seed linkage and issuing RX34 separately, then correlate to **actual player armor pixel/pass**. Exact identity failures must fail open, not apply row30 universally. Existing V451 addon remains last diagnostic single addon; V454 is offline RE only.

## Reproduce
`python research/v454/verify_v454_actor_model_seed_paths.py <original_DSR_EXE> --mutations`

The GitHub verifier composes the existing SHA-locked V453 source module, and requires the same source tree. The separately exported local V454 handoff also contains a self-contained static verifier and its full results. Checked offline with 34 exact preimages, 8 direct call edges, 5 RTTI types, 6 vtable slots, **46/46 mutation checks PASS**.
