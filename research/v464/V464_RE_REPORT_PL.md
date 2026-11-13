# V464 — model registry handle allocator, slot index and metadata

Original unmodified retail EXE SHA256 a45aaa36dd2f6cc151670a639ea5547043cf38ea79ff4178b963c6ed71f98d7b.

At 0x157D24 the FrpgModel registration path invokes allocator 0x11BA160. Native vtable+0x10 callback at 0x11BA17B precedes obtaining slot index from free list (0x11BA17E–0x11BA19F). Entry `slot_table+8*slotIndex` is loaded at 0x11BA1A3; when bit0 is set, 0x11BA1AD–0x11BA1BE forms the returned 32-bit handle as `(slotIndex & 0x00FFFFFF) | (slotEntry & 0xFF000000) | 0x80000000`. It updates the slot entry by XOR 3 at 0x11BA1C2 and stores it at 0x11BA1C6. If the bit0 test fails, the returned handle remains zero on that branch.

This confirms lower 24 bits encode a registry slot and upper bits contain slot metadata/tag. **It does NOT prove that the upper byte is a monotonically incremented generation counter.** The previous V463 decoder's low24 table lookup is consistent with this encoding; V462 typed CREATE forwards the full DWORD into FrpgModelDrawEntity+0x10.

31 exact original opcode preimages, 32/32 negative mutation tests PASS (standalone verifier in the ZIP). No EXE modifications, new addon, shader, CB, Source1/R43 or row30/31 changes. **Gameplay PlayerIns/EnemyIns owner for the six original RX34 entities remains OPEN; GPU pixel/PTDE OPEN.**

Standalone reproducible handoff: `DSRRL_V464_NATIVE_MODEL_HANDLE_ALLOCATOR_STATIC_RE.zip` SHA256 `d4ba2b052e1d0c3ae4bea6bbe4b9738dc7b574d535b5715588f023ed953dfc47`. Includes `verify_v464_allocator.py`, `V464_RE_REPORT_PL.md`, `V464_RESULTS.json`. Frozen V359-RC1 untouched.