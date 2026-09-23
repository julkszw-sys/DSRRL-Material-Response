# DSR bridge RE — CompositeOffscreenSFX routing

Date: 2026-09-23
Status: EVIDENCE_ONLY / NOT_PROMOTED
Supabase sync: PENDING_SUPABASE_SYNC

## Target
Resolve the stock DSR frame-graph position of `ImageProcessCompositeOffscreenSFX` relative to ToneMap/HDR/final combine so DSRRL can bridge PTDE-visible world/material/lighting/postprocess behavior without replacing stock spell/magic/VFX behavior or SFX-hosted PointLights.

## Provenance
Evidence recovered during DSR renderer RE from the DSR executable/control-flow investigation. Relevant function identities/addresses:

- frame/build function: `0x1402D0CA0`
- direct callsite retrieving the CompositeOffscreenSFX process/resource: `0x1402D107D`
- previously investigated graph/helper: `0x140164AF0`
- ImageProcess manager field associated with the identified process: `ImageProcessMan + 0x90`
- destination/consumer-side frame resource field in this builder: `rsi + 0xC0`

## Confirmed routing evidence
The direct callsite at `0x1402D107D` retrieves the resource/output associated with `ImageProcessCompositeOffscreenSFX` (`ImageProcessMan + 0x90`) and conditionally attaches that output to the distinct frame target represented by `rsi + 0xC0`.

Confirmed directed dependency:

```
CompositeOffscreenSFX.output  ->  target(rsi + 0xC0)
```

This means `rsi+0xC0` must not currently be treated as the CompositeOffscreenSFX object itself. It is a separate graph target/resource receiving the CompositeOffscreenSFX output as an input/dependency.

## What is NOT established
No current evidence is sufficient to identify `rsi+0xC0` as any of:

- ToneMap
- HDR combine
- final composite/present combine

Therefore no ordering claim such as `SFX -> ToneMap`, `ToneMap -> SFX`, or `SFX -> final combine` is promoted from this evidence.

Likewise, helper `0x140164AF0` alone is not sufficient to infer edge direction or semantic process identity without the downstream consumer chain.

## Bridge consequence
The stock SFX island boundary is not yet safe to cut. Replacing or bypassing `rsi+0xC0` before its semantic identity is established risks disturbing stock DSR spell/magic/VFX composition and potentially SFX-hosted PointLights. Current bridge policy should preserve this route unchanged.

## Highest-value unresolved question
Enumerate/read all downstream uses of the resource stored at `rsi+0xC0` after `0x1402D107D` and identify the first named `ImageProcess`/operator that consumes it. That consumer will determine whether CompositeOffscreenSFX is upstream or downstream of ToneMap/HDR and should expose the safe renderer-wide bridge boundary.

Recommended RE sequence:

1. Track every load of `[rsi+0xC0]` after the attachment in `0x1402D0CA0` and its immediate callees.
2. Resolve each consumer call through its object/vtable or ImageProcessMan getter.
3. Cross-reference neighboring getters (`0x140452170`, `0x140452190`, `0x1404521B0`, etc.) only after their returned process/resource identities are proven.
4. Promote an SFX/ToneMap/HDR ordering rule only when a directed producer/consumer chain is observed.
5. Preserve stock SFX families and SFX-hosted PointLights regardless of PTDE world/material bridge changes.

## Confidence
- `CompositeOffscreenSFX.output -> target(rsi+0xC0)`: HIGH
- `rsi+0xC0` is distinct from CompositeOffscreenSFX itself: HIGH
- identity of `rsi+0xC0`: OPEN
- SFX ordering relative to ToneMap/HDR: OPEN

## Canonicalization status
This note deliberately records evidence only. No semantic/operator/routing canonical should be promoted for ToneMap/HDR ordering until the downstream consumer is identified.

PENDING_SUPABASE_SYNC
