# REMO Type180 — 0x804D LightBank and packed-GI EnvDiffuse/EnvSpec RE (2026-10-21)

Target: PTDE visible behavior; host: DSR. Original EXE SHA256: `a45aaa36dd2f6cc151670a639ea5547043cf38ea79ff4178b963c6ed71f98d7b`. Exact immutable retail disassembly; no on-disk EXE modification. Owner's helmet appearance is NOT a PTDE-positive control.

## Proven route A: Type180 to addon native LightBank hook

Type180 `-1/-1` handler `0x14056F780` resolves each negative authored value from per-REMO-object default `+0xDE` (`0x140572890`), itself populated from model record byte+4 plus object area+0x24. It writes A/B to object `+0xF0/+0xF2` and blend `+0xF4`. Two internal model serializers `0x140222BD0` / `0x140219B40` send command `0x804D`. Both decode using `0x1401D7680`, storing LightBank controllers into model `+0x108`, with encoded A/B at `model+0x110/+0x112`.

Previously OPEN downstream consumer now statically VERIFIED: model-family A `0x1402167AD` passes `[rdi+0x108]` to `0x1401C0BE0 -> 0x1401C6900` (map LightBank type5); model-family B `0x140220614` passes the same `+0x108` controller and selects `0x1401C0BE0 -> 0x1401C6900` or `0x1401C0C10 -> 0x1401C6C40` (character LightBank type6 with fallback to common6/type5). Both decode model-controller A/B via their `r9+8/r9+10` and eventually call native single `0x140563B80` and blend `0x140563C30` — the EXACT two functions hooked by DSRRL. Conditional validity: if per-model default remains -1, the native output fallback is four vectors (1,0,0,0) and addon rejects a new negative donor; existing latest-source fallback can still be used. Do not claim this occurs on a particular cut.

## Proven route B: separate GI probe identity

This is a DIFFERENT operator from LightBank: at `0x14020E316/328`, FrpgModel GI owner selectors are `+0x4E/+0x4C`; at `0x14020E36B/380`, independently resolved packed IDs are `+0xDC/+0xD8`. Packed producer call `0x14020E38B -> 0x1401EB320` builds BOTH EnvDiffuse and EnvSpec from identical resolved GI A/B owner + packed IDs:
- EnvDif names `0x140563F90`, calls `0x1401EB622/66A`, bindings t11/t13;
- EnvSpc names `0x140564000`, calls `0x1401EB6DB/721`, bindings t12/t14.

The owner DSR GI binder audit additionally proves 342 matched logical identities in TPF with EnvDif resource0 and EnvSpc resource1. Thus for a single intact execution of this packed producer, the **logical probe IDs** are paired, while the two SRVs/texture bytes are necessarily different. NEVER use equality of SRV handles as a pairing test.

The GI branch at `0x14020E2FE` can instead call `0x1401EADE0`; NOT all REMO models necessarily use packed `0x1401EB320`. GI `FrpgModel+0xF0` is NOT the same ownership as Type180 REMO object `+0xF0`.

## Exact addon risk

`renderer-core/src/runtime/pmetal_envspec_draw_runtime.cpp` calls `prepare_envdiffuse(env_resources.probe_a, env_resources.probe_b, ...)`, using native **t12/t14 EnvSpec** ordinals to choose PTDE **t11/t13 EnvDiffuse**. Stock packed GI producer pairing makes this feasible, not universal or scene/draw confirmed. For the bell's actual actor, both legacy/packed paths, source identity, resource A/B and receiver must be checked at one draw before enabling a canonical patch.

Owner reports many materials becoming black and helmet differing; neither group proved PTDE-correct. Earlier owner log had 1502 READY on several routes, predominantly m14 LightBank row5, transient row0, but NO scene-cut/actor/packed-GI logical resource join. It cannot settle the mismatch.

**Status:** native code paths and producer-level paired GI identity CONFIRMED; runtime join at Quelaag receiver OPEN; final PTDE pixels OPEN. No material/LightBank change authorized. Companion local SHA-pinned binary verifier artifact: `DSRRL_REMO_ENVDIFFUSE_CONTROLLER_TO_RESOURCE_RE_20261021.zip`, SHA256 `56a02b0d94c020994c8f1b592060e486f485c9e47f412874276f50593ad3ff5b`. Full disassembly and Python verifier included.

Next narrow test: at same draw record REMO scene+cut+part/model, controller A/B, producer map/character, current LightBank bank/row, GI packed A/B keys, native t11/t13 and t12/t14 *logical* identities, PTDE bound t11/t13, shader identity, FLVER+MTD+material slot; fail-open on unverified receiver or resource association. Pixel comparison remains separate.

## Additional verified legacy-GI split (important correction)

A further complete disassembly of legacy GI producer `0x1401EADE0..0x1401EB320` **does** show writes to both EnvDiffuse `t11/t13` at `0x1401EB0A3/0C0` and EnvSpec `t12/t14` at `0x1401EB25A/277`. Any earlier claim that legacy leaves t11/t13 entirely unset was incorrect.

Crucially, *legacy* logical identity does **not** come from one packed probe ID. Its two name builders read **different fields of the same legacy record**:

- `0x140563EA2`: `movsx r9d,word ptr [rcx+0x50]` → EnvDiffuse name, verified literal `EnvDif_%s_%03d` at rdata `0x1412E9038`.
- `0x140563F32`: `movsx r9d,word ptr [rcx+0x52]` → EnvSpec name, verified literal `EnvSpc_%s_%03d` at rdata `0x1412E9018`.

By comparison, packed producer `0x1401EB320` uses the same **packed** probe ID to name both EnvDiffuse and EnvSpec. The legacy record's +0x50 and +0x52 can have **different values**; the fact they are independent does not prove a mismatch for any concrete row. Therefore native DSR legacy GI may produce t11/t12 that cannot be cross-identified using only EnvSpec packed ordinal. The correct bridge needs the actual logical EnvDiffuse identity from the exact draw or must fail open for that resource/receiver. Never infer from LightBank row equality or helmet appearance.

Reference proof script and ASM: `DSRRL_REMO_GI_LEGACY_VS_PACKED_SPLIT_RE_20261021.zip`; original EXE SHA required; runtime assignment and pixel fidelity OPEN.
