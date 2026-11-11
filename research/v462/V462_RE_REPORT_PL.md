# V462 — native FrpgModel registration handle → command 1 → FrpgModelDrawEntity+0x10

Original retail DSR EXE SHA256: `a45aaa36dd2f6cc151670a639ea5547043cf38ea79ff4178b963c6ed71f98d7b`. DSR EXE read-only; Source1/Source2, protected R43/latest_hook_source, shaders, CB, draw rows and current V359-RC1 binary unchanged.

## New exact producer/transport/consumer closure

1. `0x157D24 CALL 0x11BA160` allocates 32-bit generational native model registry handle (low24 slot, high metadata); returned EAX is copied unchanged to R8D at `0x157D2D` and EDI at `0x157D35`. That EDI returns to the FrpgModel ctor `0x157E00` and is stored at `FrpgModel+0x10` (`0x21C559`).
2. Sender sets `EDX=1` at `0x157D30`, sends exactly R8D handle to `0x11B3710` (`0x157D41`). The function writes command-state +0x10 and mode+0x14. If handle equals previously cached, bit16 in mode is set (`0x11B371D`).
3. `0x157D74→0x11B36B0` serializes command header `(payload_dwords << 17) | mode`. If bit16 is zero, also serializes handle as DWORD; otherwise omits duplicate and receiver reuses cached handle. Then `0x157DF6` sends typed Seed. Seed resource-context key comes from global `0x1D2180`, distinct from the model registration handle (V460/V461).
4. Matching command-stream decoder `0x11BE7C1..0x11BE805` reads header, extracts opcode low16 (`0x11BE7D3`), payload length shift 17 (`0x11BE7E4`), bit16 (`0x11BE7F0`) and optional new handle `0x11BE7F6→0x11BE7F9`. On opcode 1 CREATE (`0x11BE816`), it copies cached/current handle `0x11BE81B` to EBX, and at `0x11BE851` to `R8D` for Seed vtable+8 virtual call at `0x11BE858`.
5. For exactly typed `FrpgModelDrawEntitySeed` vtable `0x12CC308` slot +0x8 is factory `0x223AC0`. Factory copies R8D→EDI (`0x223AD9`), returns EDI to R8D (`0x223B06`), calls typed ctor `0x21F900` (`0x223B0F`). Through base `0x1F2DB0 → 0x11B8910`, R8D is stored into **`entity+0x10` at `0x11B891E`**.
6. Decoder stores resulting entity pointer at `table[low24(handle)]`, using `0x11BE865/867/86C/86F/873`. For follow-up opcode, parser uses existing table entry `0x11BE87E..0x11BE89D`. This table slot is not a gameplay actor ID and can be recycled across generations.

**CONFIRMED STATIC:** For a correctly routed opcode1 Seed creation, `FrpgModel` registration handle is transported to the corresponding `FrpgModelDrawEntity+0x10` without transformation. The bit16 optimization reuses the cached handle and is not an independent selection. This establishes missing semantic routing between producer and typed receiver, subject to class/stream identity.

**OPEN:** We have NOT observed full handles of six concrete V438 RX34 entities and cannot say which belongs to PlayerIns, EnemyIns, an interactive NPC or the player. We do not assert that six draw entities mean six characters, nor that NPC row31 contaminates player pixels. Source1 historical join 294/294 and two 3-entity temporal cohorts remain valid. Pixel/PTDE equivalence OPEN.

## Verification and next nonredundant runtime witness

`python research/v462/verify_v462_registration_transport.py <original_retaiI_DSR_EXE> --mutations` — 72 SHA-locked instruction preimages, 8 E8 calls, exact typed Seed vtable+8, 74/74 mutation negatives PASS. Synthetic header roundtrip is NOT game runtime. Standalone ZIP `DSRRL_V462_NATIVE_MODEL_HANDLE_TO_DRAWENTITY_REGISTRY_FULL.zip` SHA256 `fdf0965f6dc31ee06b1acf9b73b89c4e32e118a76936b7210bc1c385359ec516`.

The *next* runtime test must join **typed gameplay owner + lifetime/generation + ChrModel model handle+0x10 + matching FrpgModelDrawEntity entity+0x10 + exact RX34 row/material/receiver**, and after that GPU draw/pixel. Do NOT repeat Light current/target/weight collection, force row30, or introduce new speculative gates. No new addon built by V462.
