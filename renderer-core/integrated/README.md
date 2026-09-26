# DSRRL Core+Islands 2.0.0-dev — integrated single-addon target

This directory is the canonical integration root for the active `main`
Renderer Edition. Product/version identity comes from `VERSION.json`.

**Material Response 1.45 is frozen legacy.** It is historical provenance only
and is not linked or used as the execution layer of Core+Islands.

## Current integrated architecture

The active product is one native `renderer-core` ReShade addon. It combines
create-time shader/materialization paths with exact, reversible draw-time
transactions and operator-local diagnostic transports. Integration is not a
claim of runtime activation or PTDE-visible pixel equivalence.

Current construction includes, where their individual proof gates pass:

- the original source-complete A1 create-time islands;
- Material Response and its exact receiver/material routing;
- SpecRGB, Diffuse and Normal resource bridges;
- Subsurface routing/construction;
- Upper/Lower and HemDir3 producer/consumer carriers;
- exact P_Metal EnvSpec steady and certified HemEnvLerp routes;
- the shared conflict-checked PS/CB/SRV/sampler draw transaction layer;
- the PTDE pre-Bloom Q8 scene-sidecar resource lifecycle;
- Bloom FX/SFX diagnostic identity transport.

Every operator keeps its own receiver/material/resource/ABI gates. Failure of
one route fails open to stock DSR for that route instead of broadening the
bridge to a similar shader or material.

## Bloom / HDR diagnostic boundary

Bloom/HDR remain a separate postprocess workstream.

The addon can allocate the fixed PTDE-style 1024x720 Q8 scene sidecar and
tracks its lifecycle independently from bridge activation. Allocation alone
never authorizes scene-history writes or HDR/Bloom consumption. Exact use
still requires the complete history-preserving proof: writer-set coverage,
execution ordering, target-write recurrence, FX/SFX identity transport and
the downstream Bloom/HDR graph handoff.

The Bloom FX transport instruments the exact retail DSR Particle/Cluster
entity-to-appearance draw boundary and exposes `BLOOM_FX` telemetry. The
WaterWave diagnostic conjunction uses an exact live model-instance join plus
the same-appearance backend semantic observation and source-complete unique
WaterWave authored semantic. The `ww_diag_join` counter is **diagnostic
only**. It does not publish exact authored draw authority, does not authorize
Q8 writes and does not change pixels.

A separately published exact authored identity is still required before a
WaterWave draw can pass the Q8 writer authority gate. Bloom/HDR pixel behavior
therefore remains OPEN.

### Bloom FX diagnostic readout

The diagnostic line is tagged `_BLOOM_FX`. The decision-relevant counters are:

- `model_join=hit/miss`: live Particle appearance pointer joined to the exact
  tracked `FrpgFxParticleAppearance_Model` instance;
- `key_eq`: same-appearance backend key equalled the runtime index translated
  from WaterWave semantic ID `0xE35`;
- `ww_diag_join`: both facts above occurred on the same authenticated
  Particle appearance/model snapshot while the DSR WaterWave authored semantic
  is source-complete unique;
- `ww_publish=ok/fail`: separately published exact authored identity. This is
  expected to remain zero in the current diagnostic build;
- `ww_auth=authorized/rejected`: final WaterWave draw-authority result. A
  positive `ww_diag_join` alone must not increment `authorized`.

Thus `ww_diag_join > 0` is evidence for the remaining runtime identity join,
not permission to replay into Q8. `key_eq == 0` with semantic snapshots
present falsifies the current backend-key candidate; `model_join == 0`
instead points to the appearance-to-model join channel.

## Legacy boundary

`runtime-v1/` and Material Response 1.45 may be consulted as frozen
provenance, historical evidence, regression reference or donor evidence only.
The active Core+Islands target must not link legacy runtime libraries, compile
legacy runtime sources, install legacy hooks or use legacy generated runtime as
its execution layer. CI audits this separation.

## Safety boundary

The addon contains both create-time and draw-time native Core paths. Draw-time
mutation is performed only through scoped, reversible state transactions with
owner/conflict checks and full restore/quarantine handling. Native diagnostic
hooks are exact-executable/provenance gated and fail open on any identity or
attestation mismatch.

Bloom Q8 and Bloom FX diagnostics are deliberately stricter: the sidecar may
exist while remaining unauthorized, and semantic/model telemetry cannot
silently promote itself into writer authority.

## Status semantics

CI can establish source completeness, deterministic construction and ABI/build
compatibility. Runtime liveness, exact receiver or FX identity hits, bridge
activation, operator change and PTDE-visible pixel improvement are separate
statuses. Success at one level does not promote the next.
