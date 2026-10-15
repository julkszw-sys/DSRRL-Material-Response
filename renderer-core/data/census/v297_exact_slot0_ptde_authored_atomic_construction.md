# V297 PTDE-authored slot-0 extension — construction evidence (2026-10-15)

**Status:** SOURCE IMPLEMENTED; Win64 compile, game runtime activation and pixel behavior **OPEN**.

## Design, derived from P_Metal *transaction*, not its material response
Exact draw owner SHA/slot/MTD + retail parser-authenticated raw-MTD SHA + exact HemEnv RX33-35 + PTDE source EnvSpec semantic slot0 + live original DSR stock t1 Oct10 logical name/sidecar + full transaction prepare. Fail any gate -> stock material DSR (independent operators unchanged). No universal `exact_material`, no global PBL disable and no `_L_s` basename alias.

Authored rows (DSR retail `Mtd.mtdbnd.dcx` payload digest and PTDE donor registry):
- `P_DullLeather[DSB].mtd` raw SHA `7ea7d6ce9bb7148db62e8c27ee45edefb9547018d0b723dd4753d4c37e29f293`: PTDE c100=(0.5,0.5,0.5), c101=1.0, c102=2, EnvSpec slot0
- `P_DullLeather[DSB]_Edge.mtd` raw SHA `44e1188bafa99d49349883dcc2052c13365f196e341ca5e7c788cbad0a4e058d`: c100=(0.5,0.5,0.5), c101=1.0, c102=2, slot0
- `Ps_Body[DSB].mtd` raw SHA `af2f108831b783a43b0e02f047919719d14f38e68d6c5a97b80678d593ba1c95`: c100=(0.5,0.5,0.5), c101=1.0, c102=8, slot0.

All three source PTDE MTDs reference `FRPG_Phn_ColDifSpcBmp.spx`, as does P_Metal. Shader family equality alone is **not** pixel equivalence; dedicated slot0 constants and resource selection are applied.

Existing V291/V290 owner test with 649 sampled material draws contains 333 `NO_EXACT_PTDE_CONSUMER`; 315 are `active=0, route=0` (236 DullLeather, 79 Ps_Body). Raw runtime MTD SHA prefixes `8a577e8ec9422e91` and `217f980c5d396469` are not found among 593 MTD payload observations (546 unique SHA) in the retained DSR source MTD archives. Therefore V297 **must not** auto-authorize those observed runtime MTDs. Diagnostic logs complete SHA for these 3 name families and deliberately fails open.

**Critical fix:** V296's Oct10 material anti-hybrid guard no longer requires `decision.active` and prevents t0/t2/normal sidecar resource substitution even on inactive route0; without this, material could remain an unintentional stock-DSR-spec/PTDE-normal hybrid.

## Reproduce V297
- `renderer-core/tools/build_v297_one_atomic_addon.ps1 -ReShadeInclude <path-to-pinned-reshade/include>`
- Script applies 3 precise source modifications V296, V297 decision and V297 retail MTD parser/owner, in order and builds **one** `.addon64`.
- `renderer-core/tools/test_v297_authored_slot0_source.py` checks exact donor values, source MTD SHA, EnvSpec slot0, full SHA/slot owner gate, and inactive MR cut.
- `.github/workflows/v297-exact-slot0-atomic.yml` contains the Win64 CI recipe, but a passing workflow has **not** yet been confirmed.

Do not promote to release or claim complete 1295 atomic PTDE pixels without native build and in-game activation evidence.
