# SPC25 offline RE, 2026-10-10 — asset identity + CPU command frontier

**PTDE is the pixel target; DSR remains the host.** No runtime test, shader patch, executable disk modification, global Material Response/LightBank gain or GPU SRV reassignment was performed.

## Exact provenance

- DSR host EXE SHA-256: `a45aaa36dd2f6cc151670a639ea5547043cf38ea79ff4178b963c6ed71f98d7b`
- Uploaded DSR parts evidence ZIP SHA-256: `a76b13c909fb4a30a1a312cba7d594a851193bef55e578934829fd873f872c97`
- Canonical roster: 25 exact MTD names + original raw MTD SHA and route index, extracted from original SPC25 handoff. Only exact roster membership counts, not name substring matching.
- Offline Python corpus verification in `renderer-core/tools/verify_spc25_flver_slot_textures_offline.py`. Independent offline opcode checks are in `renderer-core/tools/verify_spc25_engine_texture_cache_offline.py`.

## New source carrier: FLVER2 material-slot texture table

FLVER2 header material/texture counts, material table offset after 64-byte dummy entries, per-material `textureIndex + textureCount`, and the **single complete** contiguous 32-byte texture-descriptor array were decoded and verified. Strings must be valid UTF16/ANSI and texture kind is the explicit `g_*` type, not a path filename guess. The full FLVER byte span is hashed SHA256. This gives an exact mapping

`(FLVER source SHA256, FLVER material slot) -> (raw virtual MTD name, g_Specular logical basename)`.

Owner evidence ZIP: **111** decompressed DFLT `partsbnd.dcx` archives; **118** valid FLVER2 models, **103** with complete nonempty texture tables and **15** with no textures; **163** parsed material slots. **25 slots** (19 distinct source FLVER SHA objects) belong to the SPC25 canonical roster, representing only **two distinct material profiles**: `P_Metal[DSB].mtd` and `P_DullLeather[DSB]_Edge.mtd`. This is incomplete asset coverage, NOT 25/25 profiles verified. The missing 23 profiles (including the problematic `C_RoughCloth`/`C_Metal` draws) must not be inferred.

Verified examples:
- `WP_A_0106.partsbnd.dcx` FLVER material slot 0: `P_Metal[DSB].mtd`, exact `g_Specular = WP_A_0106_mailbreaker_s`.
- `WP_A_0905.partsbnd.dcx` FLVER material slot 2: `P_Metal[DSB].mtd`, exact `g_Specular = WP_A_0905_s`.
- `BD_F_0000.partsbnd.dcx` FLVER material slot 1: `P_DullLeather[DSB]_Edge.mtd`, exact `g_Specular = BD_A_underwear_s`.

**Independent negative to MTD-only mapping:** `WP_A_0905_s` also belongs to `P_Leather[DSB]` in FLVER material slots 0 and 1 of the same archive. `WP_A_0106_mailbreaker_s` belongs to `P_Metal`, `P_Leather` and `P_DullLeather` in its FLVER records. More generally 15 SpecRGB basenames in this partial corpus are referenced by two or more distinct MTDs. Thus material class is not a unique GPU resource identity and material-global stock→PTDE SpecRGB substitution remains forbidden.

## New CPU intermediate operator

The **exact retail EXE** proves more than the original engine UTF16 name hash-map lookup (RVA `0x518A10`) and cache node offsets:
1. At **`0x140583BC2`**, if a resource object `RDI` and cache entry `+0x28` integer are nonzero, code takes the node integer as `ESI`, then passes the integer in `EDX` and object pointer in `R8` to **`0x14057EFB0`**. The exact `E8 REL32` call is verified.
2. The writer at **`0x14057EFC3`** sets **`EDX=0x808D`**, calls **`0x1411B3710`** (which sets packet writer descriptor fields), aligns its outgoing cursor to 8 bytes and writes the original object pointer to the payload at **`0x14057EFE2`**, then jumps to the packet finalizer `0x1411B36B0`.
3. The separate readable routine **`0x14057F000`** consumes an aligned pointer-sized payload from a cursor and stores it into a managed object's field **`+0x18`**, releasing the previous field through a refcount path. **`0x14057F08C`** prepares the related identifier **`0x808E`**. **New confirmed dispatch:** MSVC RTTI identifies the cache entry as `NS_FRPG::TexHdlResCap` (vtable `0x141380B98`) and the typed handler as `NS_FRPG::FrpgTextureEntity` (vtable `0x14137FAE8`, slot `+0x20` points directly to `0x14057F110`). This handler performs `sub edx,0x808D` at `0x14057F125`, branches at `0x14057F12B` to `0x14057F18A` and calls `0x14057F000` at `0x14057F18D`. Opcode `0x808E` takes the adjacent branch clearing `FrpgTextureEntity+0x18`. Thus the **handler-local `0x808D → 0x14057F000` pairing is confirmed**, not simply similar argument shapes. **Still unverified:** global registration of this handler on the particular renderer path, the concrete class of its managed payload, and any `ID3D11Resource` or `ID3D11ShaderResourceView` identity.

Twenty-one exact opcode anchors beyond the original 18 CPU cache anchors, four direct relative call/jump edges, two MSVC RTTI identity checks and two vtable checks confirm the named-node producer and typed `0x808D` dispatch. That gives **39 independently checked opcode anchors** plus control flow and class identity. The CPU message writer and handler-local decoder dispatch are **CONFIRMED**. Its runtime registration with a specific draw and the native GPU object edge remain **OPEN**.

## Permitted next bridge

*All preconditions are required, not alternatives:*

1. Authenticate exact FLVER SHA + material slot + raw MTD SHA + receiver/shader family, then use only the exact `g_Specular` basename read from FLVER. Asset manifests can be prepared offline.
2. Independently prove that the **actual PS t1 native SRV** used by that draw refers to the same logical stock resource, e.g. through verified command dispatch and subsequent renderer object→`ID3D11Resource`→`ID3D11ShaderResourceView` identity. **No** inference from MTD/slot, shared basename, texture dimensions, BC1, native debug label absence, or temporal proximity.
3. Only after step 2, load the existing PTDE SpecRGB sidecar and redirect **only** the targeted operator. Preserve DSR PBL roughness/alpha and all unrelated shader families; fail-open and invalidate on native resource/view destruction, rebind and conflict.
4. Validate distinct CONSTRUCTION, COMPATIBILITY, RUNTIME, BRIDGE ACTIVATION and PTDE PIXEL statuses. CI/asset equality does not establish pixel equality.

**Current decision:** do not promote or enable a guessed GPU identity bridge; existing V13 single-addon renderer/runtime untouched. Static asset material identity is **CONFIRMED for the listed two MTD profiles**; CPU packet writer and handler-local typed decoder **CONFIRMED**, external handler registration, exact native GPU SRV and PTDE pixels **OPEN**.

Run read-only local asset audit:

```bash
python renderer-core/tools/verify_spc25_flver_slot_textures_offline.py \
  "DSRRL_FINAL_EVIDENCE_PART04(2).zip" SPC25_exact_mtd_roster.csv \
  --out SPC25_flver_material_source_manifest.json
```
