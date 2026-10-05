# DSRRL PointLight R33 — Clustered Spc pre-source fail-open

Date: 2026-10-05

## Runtime evidence

Authenticated R32 build `ba2adced14e11795809a75d234174c85829b8f8b` recorded:

- clustered producer: 1491.379 us average, 4568.100 us max;
- source capture: 2307.744 us on executing samples;
- first-four selection: 10.307 us;
- sidecar build: 0.157 us;
- draw-side prepare: 9.200 us;
- GPU cache: 0.300 us;
- carrier upload: 3.500 us;
- synchronous transaction: 6.400 us total, including 5.000 us begin/capture+mutate and 1.200 us restore.

The FLVER selector profiler showed later multi-millisecond total-time spikes while its explicitly timed ordinary selector subphases remained sub-microsecond. Source inspection proves the omitted interval contains `clustered_pnts_selector_event_bridge` and `clustered_pnts_selector_identity_event_bridge`; the latter performs first-four/source reconstruction. Therefore the selector spikes and PointLight producer cost are the same nested hot path.

Runtime activation observed clustered Spc on the 480x270 reflection pass.

## R33 change

Existing project PROTECT `protect.renderer.pointlight.clustered_spc_no_partial_legacy_specular_hybrid_v1` requires stock DSR fail-open for clustered Spc until the complete PTDE local-specular microfacet window is owned.

R33 keeps `clustered_pnts_selector_event_bridge` for cheap deterministic clearing/join, but skips `clustered_pnts_selector_identity_event_bridge` when `pointlight_spc == true`. This prevents first-four/source reconstruction, sidecar construction and replacement authority for the protected clustered-Spc path.

Clustered NoSpc behavior is unchanged.

## Scope

This is an operator-isolation/performance correction, not a PointLight equation change. It removes an unauthorized clustered-Spc hybrid before its expensive source reconstruction. Stock DSR renders protected clustered Spc. NoSpc remains on the existing bridge.

Runtime performance and pixel behavior of the new build remain separate validation stages.
