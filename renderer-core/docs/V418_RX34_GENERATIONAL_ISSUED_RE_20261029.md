# V418 — RX34 generational native model issued-draw identity join

2026-10-29 · Renderer Edition · static source RE and single-addon diagnostics.
Base branch: V417 source commit 439cef4be22e441d754d5c7806c4f9fa185b7165.
Frozen active production V359-RC1 unchanged. No user executable modified.

## Confirmed source-chain and gap

The exact DSR native chain previously recovered:
typed model (V415) -> reciprocal FgModel parent (V416) -> native 32-bit FgModel generational registry handle (V417) -> native LightBank producer sequence. The prior V417 program logged the typed model at producer selection and separately logged V411 issued RX34 draw, but the issued draw's V411 journal witnessed only FrpgDrawContext, native wrapper/manager and LightBank. Comparing nearby lines by timestamp cannot establish producer-to-draw actor/model ownership.

Source audit found the V411 journal's per-context history depth is **8**; other actors' source selections may interleave or cause older snapshots to be evicted. V411 matched_snapshot is an exact observation scoped to one owner, sequence, selector epoch, wrapper and manager, but not native game actor. V411 issued trace is emitted only after the transaction result of dispatch_island_draw_indexed_batch indicates an issued draw; this is NOT a real frame/RTV/presented-pixel or PTDE match.

## V418 narrow diagnostic: eliminate temporal log correlation

* In V411 producer journal struct, add fields: validated FlverModel pointer/class/material index, reciprocal FgModel parent, native raw registry handle, registry readable and allocated bit.
* At producer publication, read same-thread V415/V416/V417 typed TLS model stamp only if its FrpgDrawContext matches the current owner; copy identity fields to **the exact V411 journal record**. The production LightBank source, cache keys, Source1/Source2 policy and resource/shader operations are unchanged.
* Registry unreadable != handle zero. V418 explicitly propagates the registry-readable flag. A positive generational certificate requires readable handle with bit31 allocated; low 24 bits are registry slot, bits24..30 generation modulo128. This is not gameplay actor identity.
* If two records for the **same** native producer sequence + epoch, wrapper, manager and LightBank differ in model/parent/generation, V418 journal resolves status ambiguous_context_identity, never accepts one arbitrary model.
* At the *same issued RX34 observation* that currently emits V411 logging, V418 reads the prior exact journal result and emits [DSRRL V418 RX34 MODEL ISSUED JOIN]. The log preserves exact FLVER and raw MTD SHA, shader pointer, route345/material slot, source1 row, owner, producer serial, model pointer, parent, generation, producer cache readback, matched context run, etc.

The exact_renderable_join flag can be positive only if:
1. source_kind==Source1, V411 status matched_snapshot and candidates==1;
2. model pointer is authenticated via exact native selector, parent reciprocal is verified, registry is readable/allocated;
3. latest Source1 cache readback's native producer sequence matches issued draw, source row matches native journal row;
4. matched snapshot was not superseded by newer journal history.

This is an **observational join to a native renderable and the issued draw**, not actor ownership, render-target/CB or pixel equivalence. Missing/evicted/ambiguous records produce a negative certificate, never select an alternate source, never change pixels.

## Static original executable evidence retained

Pinned retail DSR SHA256 a45aaa36dd2f6cc151670a639ea5547043cf38ea79ff4178b963c6ed71f98d7b, preverified FgModel/FgFlverModel vtables, ctor/dtor, registry pool slot+generation, and selector model+0x88 ABI. These do not globally cover all FlverModel/actor callsites.

## Falsifiable test with new ReShade log

In V418 only, stand in direct sunlight and rotate camera without moving your character. The exact RX34 row30 vs row31 records can now be grouped by: identical shader/FLVER/MTD/route/slot/context, plus (model,this FgModel parent, registry slot, generation).
* Strict row30+row31 **same model/parent/slot/generation** => no evidence of separate renderable owners for those events; investigate pass/RTV/CB and shader consumer.
* Strict row30+row31 **different model/parent/slot/generation** => model alias under shared context, but still not automatic evidence that visible armor received wrong actor's row.
* Non-strict records, zero/invalid handle, conflicting seq/epoch, history evicted => model source unresolved; stock DSR remains unchanged.

Construction/CI and in-game test separately graded. RUNTIME V418 OPEN until actual log. PTDE pixel OPEN. R43 protected source2 and other independent operators unchanged.

Reproduce Windows build:
PowerShell ./renderer-core/tools/build_v418_rx34_generational_diagnostic.ps1 -ReShadeInclude <path to pinned ReShade SDK>/include
Offline analyzer:
python3 renderer-core/tools/analyze_v418_rx34_issued_model_log.py ReShade.log -o v418_rx34_join.json
Native journal unit test:
g++ -std=c++17 -O2 -Wall -Wextra -Wpedantic -Werror -pthread -DDSRRL_V418_RX34_GENERATIONAL_ISSUED_JOIN -I renderer-core/include renderer-core/tests/v418_rx34_model_journal_tests.cpp -o /tmp/v418-journal && /tmp/v418-journal
