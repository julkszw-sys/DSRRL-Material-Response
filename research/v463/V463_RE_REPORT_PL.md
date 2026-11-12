# V463 — RX34 full handle vs low24 registry index; V438 kind16 v00 falsifier

Original DSR EXE SHA256 `a45aaa36dd2f6cc151670a639ea5547043cf38ea79ff4178b963c6ed71f98d7b`, V438 CSV SHA256 `744d3af90a4f99eff8e7d22d8e78c2c6ced2e09141bda3ea0a23906d3e7d4329`. No patch, no addon change.

Native follow-up decoder at `0x11BE87E` loads full DWORD handle. `0x11BE882` compares full handle against cached EBX; equal branch `0x11BE884` reuses cached RDI. Otherwise `0x11BE886..0x11BE891` takes handle and masks it with `0x00FFFFFF` for tagged negative handles; `0x11BE896..0x11BE89D` looks up entity pointer in table indexed by low24. `0x11BE8A1..0x11BE8A7` calls entity vtable+0x10. Earlier V462 CREATE sends full handle to typed factory, stores it in entity+0x10 and stores pointer at registry[low24]. **No separate generation comparison in this lookup basic block**, but downstream generation validation is UNKNOWN, not disproven. 31 exact instruction witnesses, 32/32 negative mutations PASS.

Exact SHA-locked original V438 kind16 event census shows six native entity pointers, constant packed LightBank `0x51E` (row30) or `0x51F` (row31), same FLVER `0x0E897FC0`. However `kind16.v00` changes on **every observed event of the same entity**, so it cannot be a stable native `entity+0x10` handle:
- 0x2189D9F0 row30: 94 kind16 events / 94 distinct v00 values (0x27→0x506), frames930–976
- 0x218D2070 row30: 36/36 (0x80A→0xA6F), frames1001–1018
- 0x218D25D0 row31: 82/82 (0x29→0x473), frames930–970
- 0x218D2B30 row31: 84/84 (0x2A→0x48C), frames930–971
- 0x218D4B70 row31: 34/34 (0x871→0xAB1), frames1004–1020
- 0x218D50D0 row31: 36/36 (0x84F→0xAB0), frames1003–1020.

This falsifies a tempting retrospective reinterpretation of v00 as actor/registry ID. Historical CSV **does not capture** entity+0x10, model+0x10 or typed ChrIns gameplay owner. Two temporal three-entity cohorts V461 remain valid; 6 entities != 6 gameplay actors.

Minimal differentiating runtime witness once exact owner read path verified: per RX34 issued entity, read-only (entity ptr, full entity+0x10, matching model ptr, full FrpgModel+0x10, typed ChrIns owner ptr/type and lifetime generation, native row, shader/material receiver). Require full generational equality, exact receiver family and uniqueness; UNKNOWN/fail-open otherwise. No Light current/target repeat, no row30 forcing, no draw-time patch. Runtime actor identity OPEN, GPU/pixel OPEN, PTDE visible OPEN. Frozen V359-RC1 unchanged.

Reproduce:
```sh
python research/v463/verify_v463_handle_lifetime_full.py <original_DSR_EXE> --mutations
python research/v463/audit_v463_rx34_id_gap.py <original_V438_CSV>
```
Standalone handoff ZIP `DSRRL_V463_RX34_HANDLE_LIFETIME_IDENTITY_FALSIFIER_FULL.zip` SHA256 `f7edae8197976e6059f0d00850cee5fd286deb4f86fe76fc0ce819fbc44f8623`, includes original V462 standalone PE mapper, V463 static verifier, CSV auditor, report, results and manifest. GitHub verifier uses V462 research dependency.