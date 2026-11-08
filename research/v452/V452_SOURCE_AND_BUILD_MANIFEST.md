# V452 RE lineage and reproduction

- Base: V451 single-addon source branch agent/v451-native-model-resource-rx34-20261108.
- V452: OFFLINE RE ONLY. No new addon binary; the canonical V451 single .addon64 remains unchanged. No changes to original EXE.
- Reference executable SHA256: a45aaa36dd2f6cc151670a639ea5547043cf38ea79ff4178b963c6ed71f98d7b.
- Full standalone audit package (reported in conversation): DSRRL_V452_CHRINS_OWNER_LIGHTBANK_STATIC_RE_FULL.zip SHA256 c3514d42abff4e7d2a384cd058bb13fe03f1c9cee1e659a5edcd2e5dba0840d2.
- Full offline verifier inside archive: verify_v452_chr_owner_graph.py SHA256 16987f5693cf88368943854b0d73fbc7a6176138458af374336446b7be5bb07c; helper verify_v444_actor_lifecycle.py included.
- Audit counts on retail original: 78 exact instruction preimages, 14 aligned E8 calls, 7 RTTI, 4 vtable, 91/91 mutation controls PASS.
- Repo verifier `verify_v452_owner_writer_delta.py` runs V451 SHA-locked verifier and adds independent checks of EnemyIns/PlayerIns/ChrModel ownership and owner-local row writers and category-1/5 resolver, with raw mutation tests.
- Run full offline audit:
  `python verify_v452_chr_owner_graph.py "DarkSoulsRemastered.exe" --mutations`
  using the exact original binary and helper in the archive. Never modify the EXE.
- No runtime player-vs-NPC identity, no owner-to-receiver/pixel certification, no row30/31 force, no changes to Source1 or protected R43.
- Statuses: STATIC RE confirmed; V452 addon construction NOT APPLICABLE; actor runtime OPEN; bridge activation OFF; PTDE pixel OPEN.
