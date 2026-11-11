# V461 — Actor/model registration ID vs shared Seed resource context and RX34 cohorts

Original read-only DSR EXE SHA256: a45aaa36dd2f6cc151670a639ea5547043cf38ea79ff4178b963c6ed71f98d7b. Original V438 runtime CSV SHA256: 744d3af90a4f99eff8e7d22d8e78c2c6ced2e09141bda3ea0a23906d3e7d4329. No EXE, addon, row30/31, shader, Source1/2, or R43 changes.

## 1. Correct prior mislabelled "generated Seed ID"

Function RVA 0x1D2180 only reads global pointer [RIP+0x1A9A6E1] then 32-bit [RAX+0x10], returns; it does NOT increment or allocate a fresh actor ID. At 0x157D11 caller obtains EAX, retains R15D at 0x157D1A, sends to Seed ctor at 0x157DDD/0x157DF6. Seed+0x10 written at 0x15A97E and read at 0x1F2DE6 for resource lookup 0x11B3D50, with resource stored entity+0x20. This is global-context-sourced resource/Seed lookup ID, NOT a unique owner identifier. Global value may itself vary over time.

## 2. Exact per-instance handle transport candidates

Producer FrpgModel registration: RVA 0x157D24 calls 0x11BA160, EAX return copied to EDI at 0x157D35, returned as EAX at 0x157E00, written to FrpgModel+0x10 at 0x21C559. Command 0x804C producer later reads this model registration handle at 0x21CAA1.

Receiver FrpgModelDrawEntity: factory 0x223AC0 preserves R8D at 0x223AD9 and forwards R8D at 0x223B06 to ctor 0x21F900. Base ctor 0x1F2DB0 invokes 0x11B8910 and stores R8D at entity+0x10 via 0x11B891E, separate from entity+0x08 transport pointer and entity+0x20 resource pointer. These are stronger identity candidates than common FLVER, but exact equality model+0x10 to a specific RX34 entity+0x10 is NOT yet proven. 46 exact native instruction witnesses + 11 E8 edges and 47/47 negative mutations PASS.

## 3. Replayed exact V438 CSV: two temporal cohorts

Source1 RX34 route345 issued 294: row30=66, row31=228; 12 Source2 draws not independently certified.

- CPU frames 930–976, 47 frame buckets, 3 distinct render entities: row30 0x2189D9F0 (issued frames 962–976), row31 0x218D25D0 (930–970), row31 0x218D2B30 (930–971).
- CPU frames 1001–1020, 20 frame buckets, 3 distinct render entities: row30 0x218D2070 (1001–1018), row31 0x218D4B70 (1004–1020), row31 0x218D50D0 (1003–1020).

67 CPU frame buckets total: 8 buckets with one entity, 38 with two, 21 with three. Both rows coexist in 25 buckets. Max concurrent native render entities in any captured CPU frame is 3, NEVER six. All 6 are unique session-local pointers over the capture, NOT six certified gameplay actors. Each entity has one stable row; earlier V459 native current=target/weight zero corroborates lack of per-entity 30/31 lerp. The two row31 entities in each cohort have strongly overlapping active periods, but could be two NPCs or two receivers of one character.

## 4. Correct next inference boundary

OPEN: typed PlayerIns / EnemyIns actor generation → ChrModel → FrpgModel+0x10 producer registration handle → FrpgModelDrawEntity+0x10 receiver identity, then exact RX34 GPU pixel. Same model resource pointer, same CPU frame and material do not certify gameplay ownership or pixel overlap. H_NPC is plausible but unproven. Do not force row30, claim NPC contamination, introduce a speculative actor gate, or repeat generic Light A/B trace.

Independent reproduction:
  python research/v461/verify_v461_owner_handle.py <original_DSR_EXE> --mutations
  python research/v461/audit_v461_rx34_cohorts.py <original_V438_CSV>

Static and historical CSV PASS; no new addon build, no new runtime/pixel claims. Frozen V359-RC1 source commit 6e852cdc9085dd175159e564e223e0cc8589c535 unchanged. Standalone ZIP SHA256 625ea366171152ae22a2e4b1f90b3bc83782b6277fd94768d79d32f4f999b9df.
