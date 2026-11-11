# V460 — ChrIns / EnemyIns / PlayerIns / ReplayGhostIns and NPC-talk class evidence

Original retail DSR EXE exact SHA256: `a45aaa36dd2f6cc151670a639ea5547043cf38ea79ff4178b963c6ed71f98d7b`. Read-only MSVC x64 PE RTTI, no binary or addon patch. V459 one-addon diagnostics unchanged, R43/Source1/Source2 unchanged.

## Exact native RTTI census

Enumerated 6017 type descriptors in `.data` and 6089 MSVC CompleteObjectLocator descriptors in `.rdata` via signature, class hierarchy and self-relative RVA cross-check. Exactly four hierarchies contain `ChrIns@NS_FRPG`:

| CompleteObjectLocator RVA | Type | Class chain |
|---|---|---|
| `0x16499C0` | `ChrIns` | ChrIns → FieldInsBase |
| `0x164A808` | `EnemyIns` | EnemyIns → ChrIns → FieldInsBase |
| `0x164AD48` | `PlayerIns` | PlayerIns → ChrIns → FieldInsBase |
| `0x164B0D8` | `ReplayGhostIns` | ReplayGhostIns → PlayerIns → ChrIns → FieldInsBase |

`ChrModel` belongs to a separate `ChrModel → FrpgModel` hierarchy. Neither `ChrModel` nor `EnemyIns` encodes NPC morality or dialogue capability from its type name alone.

`NpcTalkIns` is present **once** in the original EXE as part of a `DbgMenuEzCbItem<NpcTalkIns,int>` **debug-template RTTI**. No independent `.?AVNpcTalkIns@NS_FRPG@@` polymorphic RTTI/COL was observed. This absence does NOT prove that `NpcTalkIns` is not a non-polymorphic component, nor that any specific talkable NPC is an EnemyIns.

**CONFIRMED STATIC:** exact inheritance census. **OPEN:** whether talkable NPCs instantiate EnemyIns or bare ChrIns / non-RTTI type; actual `NpcTalkIns` object ownership, `ChrModel` to gameplay actor lifetime/generation, concrete RX34 entity owners, GPU pixel attribution. The working hypothesis (not promoted) is that dialogue interaction and actor class are orthogonal subsystems.

## Targeted next evidence

Trace `WorldTalkManImpl / WorldBlockTalk / WorldAreaTalk` target pointer to `ChrIns*`, check `ChrInsFactory` factory branches, classify an actual interactive NPC and an enemy at runtime using same type check. Only then bridge that actor generation to its `ChrModel`, `FrpgModelDrawEntity` and RX34 issued resource. Do not split whole-character LightBank by equipment, and do not repeat current/target Light LERP measurements already falsified in V459.

Reproduce with `python research/v460/verify_v460_chrins_rtti.py <SHA-locked-retail-EXE>`. Research-only, no new addon; construction/runtime/pixel categories remain independent. Local ZIP SHA256 `7f18d1c1d3e6e6a69bffa5b8c917c1dff03969e4a12a8fe08c2bd89618880672`.