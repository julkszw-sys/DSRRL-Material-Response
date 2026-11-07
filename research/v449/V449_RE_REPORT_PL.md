# DSRRL V449 — statyczny natywny draw D3D11 zachowuje ramkę buildera

**Data:** 2026-11-07. **Operator:** actor-specific LightBank owner → FrpgDrawParam → exact RX34. **Host:** Dark Souls Remastered. **Target:** PTDE-visible behavior. **Status:** oryginalny EXE **STATIC PASS**; runtime/bridge/pixel **OPEN/OFF**. Nie tworzono nowego addona, nie zmieniano oryginalnego EXE.

## Wniosek

Najważniejszy brak V448 to pytanie, czy podczas callbacka ReShade `draw_indexed` stos natywnego modelu jeszcze żyje. Oryginalny EXE dostarcza mocnego, szczegółowego dowodu pośredniego: `FrpgModelDrawEntity` wywołuje metodę modelu, ta **przed powrotem** sama wykonuje natywne wywołanie metod vtable D3D11 dla drawów indexed oraz indexed-instanced. Identyfikator pakietu i źródłowy `FrpgDrawParam` mogą więc potencjalnie być odzyskiwane z aktywnego stosu, bez utrzymywania procesu-globalnego „ostatniego row”. Należy jednak nadal odróżniać to od rzeczywistego sukcesu `RtlVirtualUnwind` w runtime, exact aktora zbroi i równoważności PTDE.

## Natywna ścieżka i dokładne instrukcje

Wszystkie adresy poniżej są RVA w oryginalnym `DarkSoulsRemastered.exe` SHA256 `a45aaa36dd2f6cc151670a639ea5547043cf38ea79ff4178b963c6ed71f98d7b`, VA = `0x140000000 + RVA`.

```
FrpgModelDrawEntity::default builder     0x2200D0
  RDI = entity, packet = RBP-0x30, dp = entity+0x108
  0x220CED: call qword ptr [rax+0x18]
           (verified FlverModel/FgFlverModel vtable +0x18 -> 0x20DDA0)
  0x220CF0: ORIGINAL builder continuation AFTER virtual model draw

FlverModel::draw                      0x20DDA0
  0x20DDFF: MOV R14,[R15]           (R15 is incoming native packet RDX)
  0x20E014: CALL 0x22BA20           (already-hooked native material selector)
  ... model/material/CB state ...
  0x20E540: MOV RDI,[R14+0x90]      (draw context wrapper)
  0x20E55A: MOV RCX,[RDI+0x20]     (candidate native D3D11 context)

  instanced branch:
  0x20E593: MOV EDX,ESI             (index count)
  0x20E595: MOV RAX,[RCX]           (interface vtable)
  0x20E598: CALL [RAX+0xA0]        (ID3D11DeviceContext::DrawIndexedInstanced, slot 20)
  0x20E59E: JMP 0x20E5C4           (common post-draw)

  non-instanced branch:
  0x20E5B2: MOV RCX,[RDI+0x20]     (candidate native D3D11 context)
  0x20E5B9: XOR R8D,R8D            (first index)
  0x20E5BC: MOV EDX,ESI            (index count)
  0x20E5BE: MOV RAX,[RCX]
  0x20E5C1: CALL [RAX+0x60]        (ID3D11DeviceContext::DrawIndexed, slot 12)
  0x20E5C4: shared post-draw ...
```

Model selector at `0x20E014`, instanced at `0x20E598`, non-instanced at `0x20E5C1` and common post-draw at `0x20E5C4` are all inside **the same original Win64 `.pdata` function/unwind region** `(0x20DE68,0x20E652,0x174B548)`. Parent builder virtual call `0x220CED` and original builder continuation `0x220CF0` both belong to `(0x220C51,0x220D50,0x174F5C8)`. This is a substantial improvement in the V448 actor-to-issued-receiver provenance model: the original draw is synchronous along this codepath and has not returned to the builder before the native D3D11 method call.

## Reproducible verification

```
python renderer-core/tools/verify_v449_flver_to_d3d11.py DarkSoulsRemastered.exe --mutations --json V449_D3D11_DRAW_STATIC.json
```

The script requires only Python standard library and existing `verify_v448_native_builder.py` from the same repo tools directory, using its exact SHA-locked PE parser. Checks: **19 exact instruction preimages PASS**, two COM vtable method indices `(12,20)`, instanced/non-instanced branch structure, two complete `.pdata` frame equality checks, **19/19 deliberate in-memory negative mutations**. The on-disk EXE remains unchanged.

## Exact limits

- Confirmed from original machine code: direct virtual draw call sites, original model-select call, context-pointer loads, index/count registers, branch convergence, same native model-draw and builder unwind frames.
- High-confidence typed identification (ABI): D3D11 virtual method at slot12 = `DrawIndexed`, slot20 = `DrawIndexedInstanced`; typed native COM runtime pointer not yet dereferenced in game to verify vtable identity.
- **Not proven**: same event uses original D3D draw and the intended RX34 shader for visible armor; ReShade callback actually presents the same intact original builder frame and correct `RtlVirtualUnwind`; same-gameplay-actor identity/generation; PTDE-visible output.
- Two different actors may legitimately have row30 and row31. Neither row is globally disabled, remapped or brightened. Source1 `FrpgDrawContext`-keying and protected R43 unchanged. ASM alternative model builder remains a separate family, no global whitelist expansion.

## Practical consequence for the next diagnostic

V448 contains `v448::probe_native_builder()` for **three** observation points. In the first controlled runtime, classify `V448_SELECTOR_FRAME` (kind18) and `V448_ISSUED_FRAME` (kind19): `(native sequence, entity, dp, packet, model, frame RBP, context, A/B/beta, row, receiver shader)`. If issued RX34 retains this frame, two models sharing the same context can be separated deterministically within the same live invocation, without actor lifetime cache reuse. If not, fail-open and investigate a different transport cut (recording command lists, deferred context or TLS stage loss) instead of forcing row30.

**No active LightBank correction until receiver-level evidence confirms the mapping.** The current integrated addon remains **V448 SHADOW** built from GitHub commit `09fcf4ade417e7fe75fddd490e4716d28914dd75`, CI run `42415968630`, SHA256 `03dbb1811df57e2734e89b6a39167a684c135db708d5656f7c79cee3704a0339`. Frozen V359-RC1 root intact.