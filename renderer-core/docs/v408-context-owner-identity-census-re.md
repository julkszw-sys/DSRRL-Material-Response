# V408 — FrpgDrawContext selector identity vs LightBank producer / issued GPU draw

**Baseline:** V407 native lifecycle RE; exact DSR EXE SHA256 `a45aaa36dd2f6cc151670a639ea5547043cf38ea79ff4178b963c6ed71f98d7b`. This branch enables an observational read-only census in the existing Source1 selector hook. It does not replace native EnvSpec packers, change RGB, ban any row, change Source1/Source2 eligibility, or patch `DarkSoulsRemastered.exe` on disk.

## Newly falsifiable live ABI hypothesis

At an already authenticated native selector opportunity, read the first pointer of `owner` as a candidate vtable, then compare (informational only) to the RTTI-qualified `FrpgDrawContext` vtable `module_base+0x12E6B60`. Read `owner+0x2320` and compare to LightBank manager reached through `owner+0x2318 → wrapper+0x40`. Read numeric type5 bootstrap fallback DWORD at `owner+0x2338`. Both reads use the existing safe-read VM guard and execute only in a bounded logging block after the parent, descriptor, endpoint, manager and source-pointer checks have succeeded.

Log marker `[DSRRL V408 CONTEXT SELECTOR CENSUS]`; fields: `owner`, `owner_vtable`, `frpg_draw_context_exact_vtable`, `wrapper`, `manager`, `context_manager`, `manager_matches`, `fallback_type5_id`, native endpoints, selected selectors, beta, epoch, thread ID. First 64 observations and each 8192nd thereafter only; logging requires existing `telemetry::effect_enabled()`.

The exact native packer witness path also emits `[DSRRL V408 SOURCE1 PACKER JOIN]`: owner, native bank pointer selectors, beta, `native_producer_seq`, selector epoch and a **readback of the actual actor cache** (may be superseded by another selection). Only `cache_readback=1 same_producer=1` attests that *at sampling time* the actor-indexed Source1 record matches that native producer sequence/row. It does not attest an issued GPU draw. `V388 STABLE GPU JOIN` already logs `actor_owner`, Source1/Source2 kind, bank/row and `native_producer_seq` at `indexed_issued` for bounded RX33–RX35 stable draws.

## Native detour safety RE

The existing `prepare_source_hook()` copies exactly 14 bytes without instruction-length disassembly or RIP-relative relocation. At native constructor `0x1EA460`, first complete sequence covering 14 bytes is **19 bytes** (instructions lengths 5,1,4,9). At native deleting destructor `0x1EC480`, first complete sequence covering 14 bytes is **17 bytes** (5,1,4,7) with final `lea rax,[rip+...]`; relocating these literal bytes into an arbitrary trampoline would also change its target. Therefore a naïve 14-byte entry detour for either is **REJECTED**. The V408 census uses neither new native ctor/dtor hooks nor unsafe trampolines. The native lifecycle-generation question remains OPEN.

## Interpreting results without overclaiming

- Nonmatching exact vtable does not disprove FrpgDrawContext ABI: derived type/vtable or owner identity mismatch must be investigated separately.
- `manager_matches=0` falsifies the simplistic assumption `owner+0x2320` and `wrapper+0x40` are the same manager at that selector; it does not authorize Source1 veto.
- Matching pointers do **not** prove a unique actor lifetime. Different FLVER records under one owner may be composite equipment.
- Event `native_producer_seq` is not per-actor generation and cannot alone certify a draw.
- Source2 `latest_hook_source()` TLS/synchronized fallback and consumer call remain PROTECTED and unchanged. No runtime activation or PTDE pixel equivalence demonstrated by construction.

## Reproduction and statuses

- Source: `src/runtime/pmetal_env_source_runtime.cpp`, compile flag `DSRRL_V408_CONTEXT_IDENTITY_DIAG` in `tools/build_v230_production.ps1`.
- Regression: `python3 renderer-core/tools/test_v408_context_identity_census.py`; inherited native Windows single-addon CI `.github/workflows/v408-context-identity-census.yml`.
- Independent static EXE verification kit `DSRRL_V408_OWNER_CENSUS_RE_AUDIT.zip` verifies prolog instruction boundaries and RTTI.
- Construction source contract PASS; native addon build **OPEN** pending CI; selector/runtime liveness **OPEN**; RX34 receiver/source-to-draw timing and GPU CB **OPEN**; flicker & PTDE pixel equivalence **OPEN**.
