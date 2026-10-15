# V276 strict LERP-ONLY discrepancy filter (owner correction, 2026-10-15)

**Rule:** A feature which is constant within the same exact MTD SHA + semantic hash + full FLVER SHA + slot across Stable and Lerp (such as missing t10/t12 for Leather Alp/Edge) is **NOT** accepted as the proximate transition-only trigger. A constant baseline mismatch may affect susceptibility, but is not itself the Lerp transition operator.

Original owner log `ReShade(20261015-032849).log` SHA256 `7aea775c79e6a5c7fc77971ea9a5322558826644ec8842be3ecfb57c2e7246eb`. Analyzed source-stage `BATCH_PREPARED` 621 rows, **35 exact crossphase paired physical materials**, 490 Lerp draws in all 14 source atmosphere-state bins.

| Observed quantity on matching 35 tuples | Crossphase changed | Interpretation |
| --- | ---: | --- |
| Original D3D11 **host-bound** PS interface | **35/35** | Lerp host PS object differs from Stable (even when RX stays 33); not evidence by itself of different output values |
| **Planned** addon replacement PS interface | **35/35** | Source confirms separate Stable and Lerp shader materializers/namespace; first major operator-equation investigation |
| Planned PTDE SpecRGB t10 SRV | **0/35** | No candidate in this source-level transition filter |
| Addon PS replacement bool, planned t10/t12 bool, MR decision | **0/35** | No transition activation flip |
| Authored PTDE c101,c102, CPU-shadow cb85, material/slot gate | **0/35** | No producer/authoring discontinuity shown here |
| Planned PTDE t12 SRV object set | **33/35** | Stable covers different probe endpoint resources; Lerp one A/B bridge object per material; expected topology difference, NOT automatically wrong |
| Host-bound t1 resource | **2/35** across ALL Stable versus Lerp states, but **0/35** when restricting Stable RX33 ↔ Lerp RX33 | Variation is Stable additional receiver-family samples, not proven transition fault |
| Fail-open in Lerp | **0/490** | Exclude known 22 Stable-only PREPARE_FAIL as Lerp-trigger |
| Planned PS and t10/t12 objects within sampled Lerp bins | **0/35** | No source-object switch inside transition |
| Physical GPU shader register, preFog RGB, first pixel difference | **UNOBSERVED** | No conclusion yet |

**Strong candidate from source, not diagnosed pixel root:** stable PTDE-normal-basis R5 is explicit in `pmetal_rgba_materializer.cpp`, while Lerp `pmetal_rgba_lerp_materializer.cpp` has no homologous named R5 patch. Same is true for standalone stable R7 shadow and R11 SAO, while postmerge R3 responsibilities may be moved into Lerp R19 (source comment explicitly states ownership differs). **Absence of named call does NOT prove absence of mathematical operator**; must verify instruction-consumer equivalence on matched receiver and normal/specular liveout. Prior canonical `renderer.hemenvlerp.v280_stable_lerp_operator_continuity_source_gate_v1` already flagged this source asymmetry.

**Not Lerp-only proof:** k135 fallback unity occurs in 39 source Lerp witnesses but lacks paired homologous Stable GPU-value measurement; do not label that behavior as transition-only without comparison. Cloth never sampled; don't generalize from Leather to all Cloth.

**Next decisive test:** exact Leather material RX33 host and replacement PS in Stable A/B and inside Lerp, original t1/t10/t12, PTDE N/T/B/reflect vector consumer, spec/EnvSpec preFog, postFog, postLS, final pixel. Identify first differing stage before authoring operator-local shader island. Retain stock independent renderer operators and fail-open.

**Status**: Exact runtime input/route crossphase continuity CONFIRMED; shader identity *object difference* CONFIRMED; first PTDE pixel discrepancy inside Lerp OPEN; no bridge patch proposed.
